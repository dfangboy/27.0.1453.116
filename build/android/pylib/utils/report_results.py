# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing utility functions for reporting results."""

import logging
import os
import re

from pylib import buildbot_report
from pylib import constants

import flakiness_dashboard_results_uploader


def _LogToFile(results, test_type, test_suite, build_type):
  """Log results to local files which can be used for aggregation later."""
  log_file_path = os.path.join(constants.CHROME_DIR, 'out',
                               build_type, 'test_logs')
  if not os.path.exists(log_file_path):
    os.mkdir(log_file_path)
  full_file_name = os.path.join(
      log_file_path, re.sub('\W', '_', test_type).lower() + '.log')
  if not os.path.exists(full_file_name):
    with open(full_file_name, 'w') as log_file:
      print >> log_file, '\n%s results for %s build %s:' % (
          test_type, os.environ.get('BUILDBOT_BUILDERNAME'),
          os.environ.get('BUILDBOT_BUILDNUMBER'))
    logging.info('Writing results to %s.' % full_file_name)

  logging.info('Writing results to %s.' % full_file_name)
  with open(full_file_name, 'a') as log_file:
    print >> log_file, '%s%s' % (test_suite.ljust(30), results.GetShortForm())


def _LogToFlakinessDashboard(results, test_type, test_package,
                             flakiness_server):
  """Upload results to the flakiness dashboard"""
  logging.info('Upload results for test type "%s", test package "%s" to %s' %
               (test_type, test_package, flakiness_server))

  # TODO(frankf): Enable uploading for gtests.
  if test_type != 'Instrumentation':
    logging.warning('Invalid test type.')
    return

  try:
    if flakiness_server == constants.UPSTREAM_FLAKINESS_SERVER:
        assert test_package in ['ContentShellTest',
                                'ChromiumTestShellTest',
                                'AndroidWebViewTest']
        dashboard_test_type = ('%s_instrumentation_tests' %
                               test_package.lower().rstrip('test'))
    # Downstream server.
    else:
      dashboard_test_type = 'Chromium_Android_Instrumentation'

    flakiness_dashboard_results_uploader.Upload(
        results, flakiness_server, dashboard_test_type)
  except Exception as e:
    logging.error(e)


def LogFull(results, test_type, test_package, annotation=None,
            build_type='Debug', flakiness_server=None):
  """Log the tests results for the test suite.

  The results will be logged three different ways:
    1. Log to stdout.
    2. Log to local files for aggregating multiple test steps
       (on buildbots only).
    3. Log to flakiness dashboard (on buildbots only).

  Args:
    results: An instance of TestRunResults object.
    test_type: Type of the test (e.g. 'Instrumentation', 'Unit test', etc.).
    test_package: Test package name (e.g. 'ipc_tests' for gtests,
                  'ContentShellTest' for instrumentation tests)
    annotation: If instrumenation test type, this is a list of annotations
                (e.g. ['Smoke', 'SmallTest']).
    build_type: Release/Debug
    flakiness_server: If provider, upload the results to flakiness dashboard
                      with this URL.
    """
  if not results.DidRunPass():
    logging.critical('*' * 80)
    logging.critical('Detailed Logs')
    logging.critical('%s\n%s' % ('*' * 80, results.GetLogs()))
  logging.critical('*' * 80)
  logging.critical('Summary')
  logging.critical('%s\n%s' % ('*' * 80, results))
  logging.critical('*' * 80)

  if os.environ.get('BUILDBOT_BUILDERNAME'):
    # It is possible to have multiple buildbot steps for the same
    # instrumenation test package using different annotations.
    if annotation and len(annotation) == 1:
      test_suite = annotation[0]
    else:
      test_suite = test_package
    _LogToFile(results, test_type, test_suite, build_type)

    if flakiness_server:
      _LogToFlakinessDashboard(results, test_type, test_package,
                               flakiness_server)


def PrintAnnotation(results):
  """Print buildbot annotations for test results."""
  if not results.DidRunPass():
    buildbot_report.PrintError()
  else:
    print 'Step success!'  # No annotation needed
