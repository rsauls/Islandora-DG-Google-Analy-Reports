<?php
// $Id$

/**
 * The admin form.
 */
function google_analytics_api_admin() {
  $form = array();

  if (google_analytics_api_check_token()) {
    $form['login_status'] = array(
      '#value' => t('<p>Currently you have granted access to a Google Analytics account.  Please choose the profile below that contains statistics for this site.</p>'),
      '#weight' => -10,
    );
    $form['logout'] = array(
      '#type' => 'submit',
      '#value' => t('Revoke Access'),
      '#weight' => -5,
    );
    $form['profile_id'] = array(
      '#type' => 'select',
      '#title' => t('Profile'),
      '#options' => google_analytics_api_admin_account_data_options(),
      '#default_value' => variable_get('google_analytics_api_profile_id', 0),
      '#description' => t('Choose your website\'s profile'),
      '#required' => TRUE,
      '#weight' => -2,
    );
    /*$profile = google_analytics_api_current_profile();
    $date = strtotime($profile->getUpdated());
    $form['profile_status'] = array(
      '#value' => '<div><em>(Profile last updated ' . date('F j, Y h:i:s A', $date) . ')</em></div>',
      '#weight' => -1,
    );*/
    $form['buttons']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save configuration'),
      '#weight' => 3,
    );
  } else {
    $form['logout_status'] = array(
      '#value' => t('<p>In order for you to access your Google Analytics account through Drupal, you need to grant access to your account.  This gives Drupal permission to contact the Google Analytics server and download statistics about your site.</p>'),
      '#weight' => -10,
    );
    $form['buttons']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Grant access to a Google account'),
    );
  }

  return $form;
}

/*
 * Returns an associative array of profile IDs and their names for an account.
 */
function google_analytics_api_admin_account_data_options() {
    $accounts = google_analytics_api_account_data();
    $options = array();
    $i = 0;
    foreach ($accounts as $account)
      $options[$account->getProfileId()] = $account->getTitle() . ' (' . $account->getProfileId() . ')';
    return $options;
}

/*
 * Admin form callback, run on submission of any buttons.
 */
function google_analytics_api_admin_submit($form, &$form_state) {
  if (variable_get('google_analytics_api_token', null)) {
    if ($form_state['clicked_button']['#value'] == 'Revoke Access') {
      drupal_set_message(t('You have revoked access to your Google account.'));
      return google_analytics_api_logout();
    } elseif ($form_state['clicked_button']['#value'] == 'Save configuration') {
      variable_set('google_analytics_api_profile_id', intval($form_state['values']['profile_id']));
    }
  } else {
    return google_analytics_api_authenticate();
  }
}

/**
 * Check for a returned token in the URL.
 */
function google_analytics_api_check_token() {
  $gapi = gapiAuthSub::checkToken();
  if ($gapi) {
    variable_set('google_analytics_api_token', $gapi->getToken());
    $accounts = google_analytics_api_account_data();
    $id = $accounts[0]->getProfileId();
    variable_set('google_analytics_api_profile_id', $id);
    drupal_set_message(t('You have granted access to your Google account.  Drupal will be able to download information from the account until you revoke access.'));
    header('Location: ' . gapiUrl::currentUrlWithoutGet());
    exit;
  }
  return google_analytics_api_current_token(true);
}