// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_factory_client.h"

#include <utility>

#include "osp/impl/quic/open_screen_client_session.h"
#include "osp/impl/quic/quic_connection_impl.h"
#include "osp/impl/quic/quic_packet_writer_impl.h"
#include "osp/impl/quic/quic_utils.h"
#include "quiche/quic/core/crypto/web_transport_fingerprint_proof_verifier.h"
#include "quiche/quic/core/quic_utils.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

QuicConnectionFactoryClient::QuicConnectionFactoryClient(
    TaskRunner& task_runner)
    : QuicConnectionFactoryBase(task_runner) {}

QuicConnectionFactoryClient::~QuicConnectionFactoryClient() = default;

void QuicConnectionFactoryClient::OnRead(UdpSocket* socket,
                                         ErrorOr<UdpPacket> packet_or_error) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionFactoryClient::OnRead");
  if (packet_or_error.is_error()) {
    TRACE_SET_RESULT(packet_or_error.error());
    return;
  }

  UdpPacket packet = std::move(packet_or_error.value());
  // TODO(btolsch): We will need to rethink this both for ICE and connection
  // migration support.
  auto conn_it = connections_.find(packet.source());

  // Return early because no connection can process the `packet` in this case.
  if (conn_it == connections_.end()) {
    return;
  }

  OSP_VLOG
      << __func__
      << ": QuicConnectionImpl processes data for existing connection from "
      << packet.source();
  conn_it->second.connection->OnPacketReceived(packet);
}

ErrorOr<std::unique_ptr<QuicConnection>> QuicConnectionFactoryClient::Connect(
    const IPEndpoint& local_endpoint,
    const IPEndpoint& remote_endpoint,
    const std::string& fingerprint,
    QuicConnection::Delegate* connection_delegate) {
  auto create_result = UdpSocket::Create(task_runner_, this, local_endpoint);
  if (!create_result) {
    return create_result.error();
  }
  std::unique_ptr<UdpSocket> socket = std::move(create_result.value());
  socket->Bind();

  quic::QuicPacketWriter* writer = new PacketWriterImpl(socket.get());
  auto connection = std::make_unique<quic::QuicConnection>(
      /*server_connection_id=*/quic::QuicUtils::CreateRandomConnectionId(),
      ToQuicSocketAddress(local_endpoint), ToQuicSocketAddress(remote_endpoint),
      helper_.get(), alarm_factory_.get(), writer, /*owns_writer=*/true,
      quic::Perspective::IS_CLIENT, supported_versions_,
      connection_id_generator_);

  if (!crypto_client_config_) {
    auto proof_verifier =
        std::make_unique<quic::WebTransportFingerprintProofVerifier>(
            helper_->GetClock(), /*max_validity_days=*/3650);
    const bool success =
        proof_verifier->AddFingerprint(quic::CertificateFingerprint{
            quic::CertificateFingerprint::kSha256, fingerprint});
    if (!success) {
      return Error::Code::kSha256HashFailure;
    }
    crypto_client_config_ = std::make_unique<quic::QuicCryptoClientConfig>(
        std::move(proof_verifier), nullptr);
  }

  auto connection_impl = std::make_unique<QuicConnectionImpl>(
      *this, *connection_delegate, *helper_->GetClock());
  // NOTE: Switch to `._openscreen._udp` when QUICHE supports. Currently, QUICHE
  // doesn't support the last component starting with `_`. Check the link below
  // for details:
  // https://source.chromium.org/chromium/chromium/src/+/main:net/third_party/quiche/src/quiche/common/platform/api/quiche_hostname_utils.cc;l=77
  std::string host = fingerprint + "._openscreen.udp";
  host.erase(std::remove(host.begin(), host.end(), ':'), host.end());
  OpenScreenSessionBase* session = new OpenScreenClientSession(
      std::move(connection), *crypto_client_config_, *connection_impl, config_,
      quic::QuicServerId(std::move(host), remote_endpoint.port),
      supported_versions_);
  connection_impl->set_session(session, /*owns_session=*/true);

  // TODO(btolsch): This presents a problem for multihomed receivers, which may
  // register as a different endpoint in their response.  I think QUIC is
  // already tolerant of this via connection IDs but this hasn't been tested
  // (and even so, those aren't necessarily stable either).
  connections_.emplace(remote_endpoint,
                       OpenConnection{connection_impl.get(), socket.get()});
  sockets_.emplace_back(std::move(socket));

  return ErrorOr<std::unique_ptr<QuicConnection>>(std::move(connection_impl));
}

void QuicConnectionFactoryClient::OnConnectionClosed(
    QuicConnection* connection) {
  auto entry = std::find_if(
      connections_.begin(), connections_.end(),
      [connection](const decltype(connections_)::value_type& entry) {
        return entry.second.connection == connection;
      });
  OSP_CHECK(entry != connections_.end());
  UdpSocket* const socket = entry->second.socket;
  connections_.erase(entry);

  // If none of the remaining |connections_| reference the socket, close/destroy
  // it.
  if (!ContainsIf(connections_,
                  [socket](const decltype(connections_)::value_type& entry) {
                    return entry.second.socket == socket;
                  })) {
    auto socket_it =
        std::find_if(sockets_.begin(), sockets_.end(),
                     [socket](const std::unique_ptr<UdpSocket>& s) {
                       return s.get() == socket;
                     });
    OSP_CHECK(socket_it != sockets_.end());
    sockets_.erase(socket_it);
  }
}

}  // namespace openscreen::osp