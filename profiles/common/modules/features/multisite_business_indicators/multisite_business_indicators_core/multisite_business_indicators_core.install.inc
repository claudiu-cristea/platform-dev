<?php

/**
 * @file
 * Helpers for the install scripts.
 */

/**
 * Get current module version.
 */
function _multisite_business_indicators_core_get_version() {
  $path = drupal_get_path('module', 'multisite_business_indicators_core') . '/multisite_business_indicators_core.info';
  $info = drupal_parse_info_file($path);
  return $info['version'];
}
