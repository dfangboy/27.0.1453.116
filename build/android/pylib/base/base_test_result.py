# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing base test results classes."""


class ResultType(object):
  """Class enumerating test types."""
  PASS = 'PASS'
  FAIL = 'FAIL'
  CRASH = 'CRASH'
  TIMEOUT = 'TIMEOUT'
  UNKNOWN = 'UNKNOWN'

  @staticmethod
  def GetTypes():
    """Get a list of all test types."""
    return [ResultType.PASS, ResultType.FAIL, ResultType.CRASH,
            ResultType.TIMEOUT, ResultType.UNKNOWN]


class BaseTestResult(object):
  """Base class for a single test result."""
  def __init__(self, name, test_type, log=''):
    """Construct a BaseTestResult.

    Args:
      name: Name of the test which defines uniqueness.
      test_type: Type of the test result as defined in ResultType.
      log: An optional string listing any errors.
    """
    assert name
    assert test_type in ResultType.GetTypes()
    self._name = name
    self._test_type = test_type
    self._log = log

  def __str__(self):
    return self._name

  def __repr__(self):
    return self._name

  def __cmp__(self, other):
    # pylint: disable=W0212
    return cmp(self._name, other._name)

  def __hash__(self):
    return hash(self._name)

  def GetName(self):
    """Get the test name."""
    return self._name

  def GetType(self):
    """Get the test result type."""
    return self._test_type

  def GetLog(self):
    """Get the test log."""
    return self._log


class TestRunResults(object):
  """Set of results for a test run."""
  def __init__(self):
    self._results = set()

  def GetLogs(self):
    """Get the string representation of all test logs."""
    s = []
    for test_type in ResultType.GetTypes():
      if test_type != ResultType.PASS:
        for t in sorted(self._GetType(test_type)):
          s.append('[%s] %s:' % (test_type, t))
          s.append(t.GetLog())
    return '\n'.join(s)

  def GetLongForm(self):
    """Get the long string representation of this object."""
    s = []
    s.append('ALL (%d tests)' % len(self._results))
    for test_type in ResultType.GetTypes():
      tests = sorted(self._GetType(test_type))
      if test_type == ResultType.PASS:
        s.append('%s (%d tests)' % (test_type, len(tests)))
      else:
        s.append('%s (%d tests): %s' % (test_type, len(tests), tests))
    return '\n'.join(s)

  def GetShortForm(self):
    """Get the short string representation of this object."""
    s = []
    s.append('ALL: %d' % len(self._results))
    for test_type in ResultType.GetTypes():
      s.append('%s: %d' % (test_type, len(self._GetType(test_type))))
    return ''.join([x.ljust(15) for x in s])

  def __str__(self):
    return self.GetLongForm()

  def AddResult(self, result):
    """Add |result| to the set.

    Args:
      result: An instance of BaseTestResult.
    """
    assert isinstance(result, BaseTestResult)
    self._results.add(result)

  def AddResults(self, results):
    """Add |results| to the set.

    Args:
      results: An iterable of BaseTestResult objects.
    """
    for t in results:
      self.AddResult(t)

  def AddTestRunResults(self, results):
    """Add the set of test results from |results|.

    Args:
      results: An instance of TestRunResults.
    """
    assert isinstance(results, TestRunResults)
    # pylint: disable=W0212
    self._results.update(results._results)

  def GetAll(self):
    """Get the set of all test results."""
    return self._results.copy()

  def _GetType(self, test_type):
    """Get the set of test results with the given test type."""
    return set(t for t in self._results if t.GetType() == test_type)

  def GetPass(self):
    """Get the set of all passed test results."""
    return self._GetType(ResultType.PASS)

  def GetFail(self):
    """Get the set of all failed test results."""
    return self._GetType(ResultType.FAIL)

  def GetCrash(self):
    """Get the set of all crashed test results."""
    return self._GetType(ResultType.CRASH)

  def GetTimeout(self):
    """Get the set of all timed out test results."""
    return self._GetType(ResultType.TIMEOUT)

  def GetUnknown(self):
    """Get the set of all unknown test results."""
    return self._GetType(ResultType.UNKNOWN)

  def GetNotPass(self):
    """Get the set of all non-passed test results."""
    return self.GetAll() - self.GetPass()

  def DidRunPass(self):
    """Return whether the test run was successful."""
    return not self.GetNotPass()
