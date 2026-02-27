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

  // Feed retention settings.
  $form['settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Feed settings'),
  );

  $form['settings']['feed_retention'] = array(
    '#type' => 'select',
    '#title' => t('Delete feed posts after'),
    '#description' => t('Incoming posts from followed accounts will be automatically removed after this period. Deletion runs during cron.'),
    '#options' => array(
      '0'         => t('Never (keep forever)'),
      '300'       => t('5 minutes (testing only)'),
      '3600'      => t('1 hour (testing only)'),
      '604800'    => t('1 week'),
      '2592000'   => t('1 month'),
      '15552000'  => t('6 months'),
      '31536000'  => t('1 year'),
      '157680000' => t('5 years'),
    ),
    '#default_value' => config_get('backdrop_federation_follow.settings', 'feed_retention'),
  );

  $form['settings']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
    '#submit' => array('backdrop_federation_follow_settings_submit'),
    '#limit_validation_errors' => array(array('feed_retention')),
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
      'username'         => _backdrop_federation_follow_strip_emoji($resolved['username']),
      'domain'           => $resolved['domain'],
      'display_name'     => _backdrop_federation_follow_strip_emoji($resolved['display_name']),
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
 * Submit handler for feed retention settings.
 */
function backdrop_federation_follow_settings_submit($form, &$form_state) {
  config('backdrop_federation_follow.settings')
    ->set('feed_retention', $form_state['values']['feed_retention'])
    ->save();
  backdrop_set_message(t('Feed settings saved.'));
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
    $output .= _backdrop_federation_follow_render_attachments($object);
    $output .= '</div>';
  }

  if ($total > 50) {
    $output .= '<p>' . t('Showing most recent 50 of @total posts.', array('@total' => $total)) . '</p>';
  }

  return $output;
}

/**
 * Render media attachments from an ActivityPub object.
 *
 * Images are displayed inline. Other document types (audio, video, etc.)
 * are rendered as labelled links to the original file.
 *
 * @param array $object
 *   The decoded ActivityPub object array.
 *
 * @return string
 *   HTML string of rendered attachments, or empty string if none.
 */
function _backdrop_federation_follow_render_attachments($object) {
  if (empty($object['attachment']) || !is_array($object['attachment'])) {
    return '';
  }

  $output = '';

  foreach ($object['attachment'] as $attachment) {
    $url = isset($attachment['url']) ? $attachment['url'] : NULL;
    if (!$url || !filter_var($url, FILTER_VALIDATE_URL)) {
      continue;
    }

    $media_type = isset($attachment['mediaType']) ? $attachment['mediaType'] : '';
    $alt = isset($attachment['name']) ? check_plain($attachment['name']) : '';
    $safe_url = check_url($url);

    if (strpos($media_type, 'image/') === 0 || $attachment['type'] === 'Image') {
      $output .= '<img src="' . $safe_url . '" alt="' . $alt . '"'
        . ' style="max-width:200px;max-height:200px;width:auto;height:auto;display:block;margin-top:0.5em;">';
    }
    elseif (strpos($media_type, 'video/') === 0) {
      $output .= '<video controls style="max-width:100%;display:block;margin-top:0.5em;">'
        . '<source src="' . $safe_url . '" type="' . check_plain($media_type) . '">'
        . '<a href="' . $safe_url . '">' . ($alt ?: t('Download video')) . '</a>'
        . '</video>';
    }
    elseif (strpos($media_type, 'audio/') === 0) {
      $output .= '<audio controls style="width:100%;margin-top:0.5em;">'
        . '<source src="' . $safe_url . '" type="' . check_plain($media_type) . '">'
        . '<a href="' . $safe_url . '">' . ($alt ?: t('Download audio')) . '</a>'
        . '</audio>';
    }
    else {
      // Unknown type — render as a download link.
      $label = $alt ?: check_plain(basename(parse_url($url, PHP_URL_PATH)));
      $output .= '<p><a href="' . $safe_url . '">' . $label . '</a></p>';
    }
  }

  return $output;
}
