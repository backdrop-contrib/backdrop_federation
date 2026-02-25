<?php
/**
 * @file
 * Admin configuration form for Fediverse module.
 */

/**
 * Admin settings form.
 */
function fediverse_admin_settings_form($form, &$form_state) {
  $config = config('fediverse.settings');

  $form['#config'] = 'fediverse.settings';

  $form['enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enable Fediverse federation'),
    '#description' => t('When enabled, this site will respond to ActivityPub requests and federate content.'),
    '#default_value' => $config->get('enabled'),
  );

  $form['actor'] = array(
    '#type' => 'fieldset',
    '#title' => t('Actor settings'),
    '#collapsible' => TRUE,
  );

  global $base_url;
  $host = parse_url($base_url, PHP_URL_HOST);
  $actor_name = $config->get('actor_name') ?: 'site';

  $form['actor']['actor_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Actor username'),
    '#description' => t('The username for this site\'s actor. Users will find this site as <strong>@%name@%host</strong>.', array(
      '%name' => $actor_name,
      '%host' => $host,
    )),
    '#default_value' => $config->get('actor_name'),
    '#field_prefix' => '@',
    '#size' => 30,
    '#maxlength' => 64,
    '#element_validate' => array('_fediverse_validate_actor_name'),
  );

  $form['actor']['actor_display_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Display name'),
    '#description' => t('The display name shown in Fediverse clients. Defaults to site name if empty.'),
    '#default_value' => $config->get('actor_display_name'),
    '#placeholder' => config_get('system.core', 'site_name'),
  );

  $form['actor']['actor_summary'] = array(
    '#type' => 'textarea',
    '#title' => t('Bio'),
    '#description' => t('A brief description of this site. Defaults to site slogan if empty.'),
    '#default_value' => $config->get('actor_summary'),
    '#placeholder' => config_get('system.core', 'site_slogan'),
    '#rows' => 3,
  );

  // Content types.
  $form['content'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content federation'),
    '#collapsible' => TRUE,
  );

  $node_types = node_type_get_names();
  $form['content']['content_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Content types to federate'),
    '#description' => t('Select which content types should be published to followers when created or updated.'),
    '#options' => $node_types,
    '#default_value' => $config->get('content_types') ?: array(),
  );

  // Status information.
  $form['status'] = array(
    '#type' => 'fieldset',
    '#title' => t('Status'),
    '#collapsible' => TRUE,
  );

  // Follower count.
  $follower_count = db_select('fediverse_followers', 'f')
    ->countQuery()
    ->execute()
    ->fetchField();

  $form['status']['follower_count'] = array(
    '#type' => 'item',
    '#title' => t('Followers'),
    '#markup' => format_plural($follower_count, '1 follower', '@count followers'),
  );

  // Keypair status.
  $keypair = fediverse_get_keypair();
  $form['status']['keypair_status'] = array(
    '#type' => 'item',
    '#title' => t('Keypair'),
    '#markup' => $keypair ? t('Generated and ready') : t('<span class="error">Missing - try reinstalling module</span>'),
  );

  // Actor URL.
  $form['status']['actor_url'] = array(
    '#type' => 'item',
    '#title' => t('Actor URL'),
    '#markup' => l(fediverse_get_actor_uri(), 'fediverse/actor'),
  );

  // WebFinger URL.
  $webfinger_url = $base_url . '/.well-known/webfinger?resource=acct:' . $actor_name . '@' . $host;
  $form['status']['webfinger_url'] = array(
    '#type' => 'item',
    '#title' => t('WebFinger URL'),
    '#markup' => l($webfinger_url, '.well-known/webfinger', array(
      'query' => array('resource' => 'acct:' . $actor_name . '@' . $host),
    )),
  );

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  return $form;
}

/**
 * Validation callback for actor name field.
 */
function _fediverse_validate_actor_name($element, &$form_state) {
  $value = $element['#value'];
  if (!empty($value) && !preg_match('/^[a-zA-Z0-9_]+$/', $value)) {
    form_error($element, t('Actor username may only contain letters, numbers, and underscores.'));
  }
}

/**
 * Submit handler for admin settings form.
 */
function fediverse_admin_settings_form_submit($form, &$form_state) {
  $config = config('fediverse.settings');

  $config->set('enabled', (bool) $form_state['values']['enabled']);
  $config->set('actor_name', $form_state['values']['actor_name']);
  $config->set('actor_display_name', $form_state['values']['actor_display_name']);
  $config->set('actor_summary', $form_state['values']['actor_summary']);

  // Filter out unchecked content types.
  $content_types = array_filter($form_state['values']['content_types']);
  $config->set('content_types', array_values($content_types));

  $config->save();

  backdrop_set_message(t('Fediverse settings saved.'));
}
