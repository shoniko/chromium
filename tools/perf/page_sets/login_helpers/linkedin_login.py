# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.login_helpers import login_utils


def LoginDesktopAccount(action_runner, credential,
                 credentials_path=login_utils.DEFAULT_CREDENTIAL_PATH):
  """Logs in into a Linkedin account.

  This function navigates the tab into Linkedin's login page and logs in a user
  using credentials in |credential| part of the |credentials_path| file.

  Args:
    action_runner: Action runner responsible for running actions on the page.
    credential: The credential to retrieve from the credentials file (string).
    credentials_path: The path to credential file (string).

  Raises:
    exceptions.Error: See ExecuteJavaScript()
    for a detailed list of possible exceptions.
  """
  account_name, password = login_utils.GetAccountNameAndPassword(
      credential, credentials_path=credentials_path)

  action_runner.Navigate('https://www.linkedin.com/uas/login')
  action_runner.Wait(1) # Error page happens if this wait is not here.
  login_utils.InputWithSelector(
      action_runner, '%s@gmail.com' % account_name, 'input[type=text]')
  login_utils.InputWithSelector(
      action_runner, password, 'input[type=password]')

  login_button_function = (
      'document.getElementsByName("signin")[0]')
  action_runner.WaitForElement(element_function=login_button_function)
  action_runner.ClickElement(element_function=login_button_function)

  search_bar_function = (
      'document.getElementsByClassName("nav-search-bar")[0]')
  action_runner.WaitForElement(element_function=search_bar_function)
