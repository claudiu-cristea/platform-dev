<?php

/**
 * @file
 * Contains HttpRequestResponse
 */

namespace Drupal\nexteuropa_integration\Backend\Response;

/**
 * Class HttpRequestResponse.
 *
 * Parse response returned by standard drupal_http_request(), without overrides.
 *
 * @package Drupal\nexteuropa_integration\Backend\Response
 */
class HttpRequestResponse extends AbstractResponse {

  /**
   * {@inheritdoc}
   */
  public function hasErrors() {
    return $this->getStatusCode() != 200;
  }

  /**
   * {@inheritdoc}
   */
  public function getErrorMessage() {
    if ($this->hasErrors()) {
      return $this->getResponse()->error;
    }
  }

  /**
   * {@inheritdoc}
   */
  public function getStatusCode() {
    return $this->getResponse()->code;
  }

  /**
   * {@inheritdoc}
   */
  public function getStatusMessage() {
    return $this->getResponse()->status_message;
  }

  /**
   * {@inheritdoc}
   */
  public function getData() {
    if (!$this->hasErrors()) {
      return $this->getResponse()->data;
    }
  }

}