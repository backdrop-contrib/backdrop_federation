<?php
/**
 * @file
 * Views integration for Backdrop Federation.
 *
 * Exposes backdrop_federation_followers as a Views base table so site builders
 * can create public follower lists, counts, and date-filtered views.
 */

/**
 * Implements hook_views_data().
 */
function backdrop_federation_views_data() {
  $data = array();

  // ---------------------------------------------------------------------------
  // backdrop_federation_followers — base table.
  // ---------------------------------------------------------------------------

  $data['backdrop_federation_followers']['table']['group'] = t('Fediverse Followers');

  $data['backdrop_federation_followers']['table']['base'] = array(
    'field' => 'id',
    'title' => t('Fediverse Follower'),
    'help' => t('Fediverse accounts that follow this site.'),
  );

  // id.
  $data['backdrop_federation_followers']['id'] = array(
    'title' => t('Follower ID'),
    'help' => t('Internal numeric ID of the follower record.'),
    'field' => array(
      'handler' => 'views_handler_field_numeric',
      'click sortable' => TRUE,
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_numeric',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_numeric',
    ),
  );

  // actor_uri — the canonical URL of the remote follower's actor document.
  $data['backdrop_federation_followers']['actor_uri'] = array(
    'title' => t('Actor URI'),
    'help' => t('The full ActivityPub URI of the follower. Enable "Output this field as a link" to link to their remote profile.'),
    'field' => array(
      'handler' => 'views_handler_field_url',
      'click sortable' => TRUE,
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
    'sort' => array(
      'handler' => 'views_handler_sort',
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_string',
    ),
  );

  // inbox_uri — delivery target, useful for debugging but rarely shown publicly.
  $data['backdrop_federation_followers']['inbox_uri'] = array(
    'title' => t('Inbox URI'),
    'help' => t('The inbox URI used for delivering posts to this follower.'),
    'field' => array(
      'handler' => 'views_handler_field_url',
      'click sortable' => FALSE,
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
  );

  // shared_inbox_uri — optional, may be empty.
  $data['backdrop_federation_followers']['shared_inbox_uri'] = array(
    'title' => t('Shared inbox URI'),
    'help' => t('The shared inbox URI for this follower\'s instance, if available.'),
    'field' => array(
      'handler' => 'views_handler_field_url',
      'click sortable' => FALSE,
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_string',
    ),
  );

  // created — when the Follow was accepted and the follower was stored.
  $data['backdrop_federation_followers']['created'] = array(
    'title' => t('Followed on'),
    'help' => t('The date this account started following the site.'),
    'field' => array(
      'handler' => 'views_handler_field_date',
      'click sortable' => TRUE,
    ),
    'sort' => array(
      'handler' => 'views_handler_sort_date',
    ),
    'filter' => array(
      'handler' => 'views_handler_filter_date',
    ),
    'argument' => array(
      'handler' => 'views_handler_argument_date',
    ),
  );

  return $data;
}
