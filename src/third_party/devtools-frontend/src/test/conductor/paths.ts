// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as os from 'os';
import * as path from 'path';

export const SOURCE_ROOT = path.join(__dirname, '..', require('../build.js').SOURCE_ROOT);
export const GEN_DIR = path.normalize(path.join(__dirname, '..', '..'));

export function rebase(fromRoot: string, toRoot: string, filename: string, newExt?: string) {
  if (!path.isAbsolute(filename) || !path.isAbsolute(fromRoot) || !path.isAbsolute(toRoot)) {
    return filename;
  }

  const relative = path.relative(fromRoot, filename);
  if (relative.startsWith('.')) {
    return filename;
  }
  if (!path.relative(toRoot, filename).startsWith('.') && toRoot.length > fromRoot.length) {
    return filename;
  }

  const rebased = path.resolve(toRoot, relative);
  if (!newExt) {
    return rebased;
  }

  const {ext} = path.parse(rebased);
  return ext.length > 0 ? rebased.substr(0, rebased.length - ext.length) + newExt : rebased;
}

export function isContainedInDirectory(contained: string, directory: string) {
  return !path.relative(directory, contained).startsWith('..');
}

export class PathPair {
  private constructor(readonly sourcePath: string, readonly buildPath: string) {
    if (!path.isAbsolute(sourcePath) || !path.isAbsolute(buildPath)) {
      throw new Error('Repo paths must be absolute');
    }
  }

  static get(pathname: string) {
    const absPath = path.normalize(path.resolve(pathname));
    if (!absPath) {
      return null;
    }

    const sourcePath = rebase(GEN_DIR, SOURCE_ROOT, absPath, '.ts');
    const buildPath = rebase(SOURCE_ROOT, GEN_DIR, absPath, '.js');

    return new PathPair(sourcePath, buildPath);
  }
}

export function defaultChromePath() {
  const paths = {
    'linux': path.join('chrome-linux', 'chrome'),
    'darwin':
        path.join('chrome-mac', 'Google Chrome for Testing.app', 'Contents', 'MacOS', 'Google Chrome for Testing'),
    'win32': path.join('chrome-win', 'chrome.exe'),
  };
  return path.join(SOURCE_ROOT, 'third_party', 'chrome', paths[os.platform() as 'linux' | 'win32' | 'darwin']);
}