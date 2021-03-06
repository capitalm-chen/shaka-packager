#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""Test wrapper for the sample packager binary."""

import os
import platform
import subprocess

import test_env


class PackagerApp(object):
  """Main integration class for testing the packager binary."""

  def __init__(self):
    packager_name = 'packager'
    if platform.system() == 'Windows':
      packager_name += '.exe'
    self.binary = os.path.join(test_env.SCRIPT_DIR, packager_name)
    assert os.path.exists(self.binary), ('Please run from output directory, '
                                         'e.g. out/Debug/packager_test.py')

  def DumpStreamInfo(self, stream):
    input_str = 'input=%s' % stream
    cmd = [self.binary, input_str, '--dump_stream_info']
    return subprocess.check_output(cmd)

  def Version(self):
    return subprocess.check_output([self.binary, '--version'])

  def Package(self, streams, flags=None):
    if flags is None:
      flags = []
    cmd = [self.binary]
    cmd.extend(streams)
    cmd.extend(flags)
    assert 0 == subprocess.call(cmd)
