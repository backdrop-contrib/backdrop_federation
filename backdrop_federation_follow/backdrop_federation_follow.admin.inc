<?php
/**
 * @file
 * Admin pages for Backdrop Federation Follow sub-module.
 */

/**
 * Admin form for managing followed accounts.
 */
function backdrop_federation_follow_admin_form($form, &$form_state) {
  $form['#attached']['css'][] = array(
    'data' => '.backdrop-federation-status-pending { color: #e6a817; font-weight: bold; }
               .backdrop-federation-status-accepted { color: #2e7d32; font-weight: bold; }
               .backdrop-federation-status-rejected { color: #c62828; font-weight: bold; }',
    'type' => 'inline',
  );

  $form['help'] = array(
    '#markup' => '<p>' . t('Follow other Fediverse accounts to receive their posts in your local <a href="!feed">feed</a>. Enter a handle below to send a Follow request — their server will send an Accept or Reject back to your inbox.', array('!feed' => url('admin/config/services/fediverse/feed'))) . '</p>',
    '#weight' => -20,
  );

  // Current following table.
  $following = db_select('backdrop_federation_following', 'f')
    ->fields('f')
    ->orderBy('created', 'DESC')
    ->execute()
    ->fetchAll();

  if ($following) {
    $header = array(t('Account'), t('Display name'), t('Status'), t('Since'), t('Actions'));
    $rows = array();
    foreach ($following as $account) {
      $status_labels = array(
        'pending'  => '<span class="backdrop-federation-status-pending">'  . t('Pending')  . '</span>',
        'accepted' => '<span class="backdrop-federation-status-accepted">' . t('Accepted') . '</span>',
        'rejected' => '<span class="backdrop-federation-status-rejected">' . t('Rejected') . '</span>',
      );
      $status = isset($status_labels[$account->status]) ? $status_labels[$account->status] : check_plain($account->status);
      $handle = '@' . $account->username . '@' . $account->domain;

      $rows[] = array(
        l($handle, $account->actor_uri),
        check_plain($account->display_name),
        $status,
        format_date($account->created, 'short'),
        l(t('Unfollow'), 'admin/config/services/fediverse/following/unfollow/' . $account->id),
      );
    }
    $form['following_table'] = array(
      '#markup' => theme('table', array('header' => $header, 'rows' => $rows)),
      '#weight' => -10,
    );
  }
  else {
    $form['following_table'] = array(
      '#markup' => '<p>' . t('Not following any accounts yet.') . '</p>',
      '#weight' => -10,
    );
  }

  // Follow new account fieldset.
  $form['follow'] = array(
    '#type' => 'fieldset',
    '#title' => t('Follow an account'),
  );

  $form['follow']['handle'] = array(
    '#type' => 'textfield',
    '#title' => t('Fediverse handle'),
    '#description' => t('Enter a handle like @user@mastodon.social. A signed Follow request will be sent immediately.'),
    '#placeholder' => '@user@mastodon.social',
    '#size' => 40,
    '#maxlength' => 255,
  );

  $form['follow']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Send Follow request'),
    '#validate' => array('backdrop_federation_follow_admin_form_validate'),
    '#submit' => array('backdrop_federation_follow_admin_form_submit'),
    '#limit_validation_errors' => array(array('handle')),
  );

  return $form;
}

/**
 * Validate handler for the follow form.
 */
function backdrop_federation_follow_admin_form_validate($form, &$form_state) {
  $handle = ltrim(trim($form_state['values']['handle']), '@');
  if (!preg_match('/^[^@\s]+@[^@\s]+\.[^@\s]+$/', $handle)) {
    form_set_error('handle', t('Please enter a valid Fediverse handle, e.g. @user@mastodon.social.'));
  }
}

/**
 * Submit handler for the follow form.
 */
function backdrop_federation_follow_admin_form_submit($form, &$form_state) {
  $handle = ltrim(trim($form_state['values']['handle']), '@');

  $resolved = _backdrop_federation_follow_resolve_handle($handle);
  if (!$resolved) {
    backdrop_set_message(t('Could not resolve @handle. Make sure the account exists and the instance is reachable.', array('@handle' => '@' . $handle)), 'error');
    return;
  }

  // Check whether we are already following this account.
  $existing = db_select('backdrop_federation_following', 'f')
    ->fields('f', array('id', 'status'))
    ->condition('actor_uri', $resolved['actor_uri'])
    ->execute()
    ->fetchObject();

  if ($existing) {
    backdrop_set_message(t('Already following @handle (status: @status).', array(
      '@handle' => '@' . $handle,
      '@status' => $existing->status,
    )), 'warning');
    return;
  }

  // Send the signed Follow activity.
  $activity_id = _backdrop_federation_follow_send_follow($resolved['actor_uri'], $resolved['inbox_uri']);
  if (!$activity_id) {
    backdrop_set_message(t('Failed to send Follow request to @handle. Check the watchdog log for details.', array('@handle' => '@' . $handle)), 'error');
    return;
  }

  db_insert('backdrop_federation_following')
    ->fields(array(
      'actor_uri'        => $resolved['actor_uri'],
      'inbox_uri'        => $resolved['inbox_uri'],
      'username'         => $resolved['username'],
      'domain'           => $resolved['domain'],
      'display_name'     => $resolved['display_name'],
      'avatar_url'       => $resolved['avatar_url'],
      'status'           => 'pending',
      'follow_activity_id' => $activity_id,
      'created'          => REQUEST_TIME,
    ))
    ->execute();

  backdrop_set_message(t('Follow request sent to @handle. The status will update to "Accepted" once their server responds.', array('@handle' => '@' . $handle)));
  watchdog('backdrop_federation_follow', 'Sent Follow request to @actor', array('@actor' => $resolved['actor_uri']), WATCHDOG_INFO);
}

/**
 * Confirm form for unfollowing an account.
 *
 * @param int $id
 *   The backdrop_federation_following row ID.
 */
function backdrop_federation_follow_unfollow_confirm($form, &$form_state, $id) {
  $account = db_select('backdrop_federation_following', 'f')
    ->fields('f')
    ->condition('id', (int) $id)
    ->execute()
    ->fetchObject();

  if (!$account) {
    backdrop_set_message(t('Account not found.'), 'error');
    backdrop_goto('admin/config/services/fediverse/following');
  }

  $form['id']               = array('#type' => 'value', '#value' => (int) $id);
  $form['actor_uri']        = array('#type' => 'value', '#value' => $account->actor_uri);
  $form['inbox_uri']        = array('#type' => 'value', '#value' => $account->inbox_uri);
  $form['follow_activity_id'] = array('#type' => 'value', '#value' => $account->follow_activity_id);
  $form['handle']           = array('#type' => 'value', '#value' => '@' . $account->username . '@' . $account->domain);

  return confirm_form(
    $form,
    t('Unfollow %handle?', array('%handle' => '@' . $account->username . '@' . $account->domain)),
    'admin/config/services/fediverse/following',
    t('This will send an Undo Follow activity to their server and remove all their posts from your local feed.'),
    t('Unfollow'),
    t('Cancel')
  );
}

/**
 * Submit handler for the unfollow confirm form.
 */
function backdrop_federation_follow_unfollow_confirm_submit($form, &$form_state) {
  _backdrop_federation_follow_send_unfollow(
    $form_state['values']['actor_uri'],
    $form_state['values']['inbox_uri'],
    $form_state['values']['follow_activity_id']
  );

  db_delete('backdrop_federation_following')
    ->condition('id', $form_state['values']['id'])
    ->execute();

  // Remove their posts from the local feed.
  db_delete('backdrop_federation_feed')
    ->condition('actor_uri', $form_state['values']['actor_uri'])
    ->execute();

  backdrop_set_message(t('Unfollowed @handle.', array('@handle' => $form_state['values']['handle'])));
  watchdog('backdrop_federation_follow', 'Unfollowed @actor', array('@actor' => $form_state['values']['actor_uri']), WATCHDOG_INFO);
  $form_state['redirect'] = 'admin/config/services/fediverse/following';
}

/**
 * Admin feed page — lists posts received from followed accounts.
 */
function backdrop_federation_follow_admin_feed_page() {
  $total = db_select('backdrop_federation_feed', 'f')
    ->countQuery()
    ->execute()
    ->fetchField();

  $output  = '<p>' . t('Posts received from accounts this site follows, stored in your local feed. Only accounts with an "Accepted" follow status will appear here.') . '</p>';
  $output .= '<p>' . format_plural($total, '1 post in feed', '@count posts in feed') . '</p>';

  if ($total == 0) {
    $output .= '<p>' . t('No posts yet. <a href="!url">Follow some accounts</a> and wait for them to post.', array('!url' => url('admin/config/services/fediverse/following'))) . '</p>';
    return $output;
  }

  $rows = db_select('backdrop_federation_feed', 'f')
    ->fields('f')
    ->orderBy('published', 'DESC')
    ->range(0, 50)
    ->execute();

  foreach ($rows as $row) {
    // Look up the actor's display info from the following table.
    $account = db_select('backdrop_federation_following', 'f')
      ->fields('f', array('username', 'domain', 'display_name', 'avatar_url'))
      ->condition('actor_uri', $row->actor_uri)
      ->execute()
      ->fetchObject();

    $handle = $account
      ? ('@' . check_plain($account->username) . '@' . check_plain($account->domain))
      : check_plain($row->actor_uri);
    $display_name = $account ? check_plain($account->display_name) : check_plain($row->actor_uri);

    $object = json_decode($row->object_json, TRUE);
    $original_url = isset($object['url']) ? $object['url'] : (isset($object['id']) ? $object['id'] : NULL);

    $meta = '<strong>' . $display_name . '</strong>'
      . ' <span style="color:#666;">' . $handle . '</span>'
      . ' &middot; ' . format_date($row->published, 'short');

    if ($original_url) {
      $meta .= ' &middot; ' . l(t('View original'), $original_url);
    }

    $output .= '<div style="border-bottom:1px solid #ddd;padding:1em 0;">';
    $output .= '<p style="margin:0 0 0.5em;">' . $meta . '</p>';
    $output .= '<div>' . $row->content . '</div>';
    $output .= '</div>';
  }

  if ($total > 50) {
    $output .= '<p>' . t('Showing most recent 50 of @total posts.', array('@total' => $total)) . '</p>';
  }

  return $output;
}
