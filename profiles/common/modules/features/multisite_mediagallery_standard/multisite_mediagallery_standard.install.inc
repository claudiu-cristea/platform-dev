<?php

/**
 * @file
 * Helpers for the install scripts.
 */

/**
 * Get current module version.
 */
function _multisite_mediagallery_standard_get_version() {
  $module = basename(__FILE__, ".install.inc");
  $path = drupal_get_path('module', $module) . '/' . $module . '.info';
  $info = drupal_parse_info_file($path);
  return $info['version'];
}
