// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/phone_number_i18n.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"

namespace autofill {

namespace i18n {

bool IsPossiblePhoneNumber(const std::string& phone_number,
                           const std::string& country_code) {
  return false;
}

bool IsValidPhoneNumber(const std::string& phone_number,
                        const std::string& country_code) {
  return false;
}

std::u16string NormalizePhoneNumber(const std::u16string& value,
                                    const std::string& region) {
  DCHECK_EQ(2u, region.size());
  return value;
}

bool ConstructPhoneNumber(const std::u16string& input_whole_number,
                          const std::string& region,
                          std::u16string* output_whole_number) {
  DCHECK_EQ(2u, region.size());
  output_whole_number->clear();

  return true;
}

bool PhoneNumbersMatch(const std::u16string& number_a,
                       const std::u16string& number_b,
                       const std::string& raw_region,
                       const std::string& app_locale) {
  if (number_a.empty() && number_b.empty()) {
    return true;
  }

  return false;
}

std::u16string GetFormattedPhoneNumberForDisplay(const AutofillProfile& profile,
                                                 const std::string& locale) {
  // Since the "+" is removed for some country's phone numbers, try to add a "+"
  // and see if it is a valid phone number for a country.
  // Having two "+" in front of a number has no effect on the formatted number.
  // The reason for this is international phone numbers for another country. For
  // example, without adding a "+", the US number 1-415-123-1234 for an AU
  // address would be wrongly formatted as +61 1-415-123-1234 which is invalid.
  std::string phone = base::UTF16ToUTF8(
      profile.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), locale));
  return base::UTF8ToUTF16(phone);
}

std::string FormatPhoneNationallyForDisplay(const std::string& phone_number,
                                            const std::string& country_code) {
  return phone_number;
}

std::string FormatPhoneForDisplay(const std::string& phone_number,
                                  const std::string& country_code) {
  return phone_number;
}

std::string FormatPhoneForResponse(const std::string& phone_number,
                                   const std::string& country_code) {
  return phone_number;
}

PhoneObject::PhoneObject(const std::u16string& number,
                         const std::string& region,
                         bool infer_country_code) {
  DCHECK_EQ(2u, region.size());
}

PhoneObject::PhoneObject(const PhoneObject& other) {
  *this = other;
}

PhoneObject::PhoneObject() = default;

PhoneObject::~PhoneObject() = default;

const std::u16string& PhoneObject::GetFormattedNumber() const {
  return formatted_number_;
}

std::u16string PhoneObject::GetNationallyFormattedNumber() const {
  return whole_number_;
}

const std::u16string& PhoneObject::GetWholeNumber() const {
  return whole_number_;
}

PhoneObject& PhoneObject::operator=(const PhoneObject& other) {
  if (this == &other)
    return *this;

  region_ = other.region_;
  country_code_ = other.country_code_;
  city_code_ = other.city_code_;
  number_ = other.number_;

  formatted_number_ = other.formatted_number_;
  whole_number_ = other.whole_number_;

  return *this;
}

}  // namespace i18n
}  // namespace autofill
