<?php

/**
 * @file
 * Contains BackendConfiguration.
 */

namespace Drupal\integration\Backend\Configuration;

use Drupal\integration\Configuration\AbstractConfiguration;
use Drupal\integration\Configuration\AbstractComponentConfiguration;
use Drupal\integration\PluginManager;

/**
 * Class BackendConfiguration.
 *
 * @package Drupal\integration\Backend
 */
class BackendConfiguration extends AbstractConfiguration {

  /**
   * Backend type plugin.
   *
   * @see integration_backend_info()
   *
   * @var string
   */
  public $type = '';

  /**
   * Formatter handler machine name.
   *
   * @see hook_integration_backend_formatter_handler_info()
   *
   * @var string
   */
  public $formatter = '';

  /**
   * Response handler machine name.
   *
   * @see integration_backend_response_handler_info()
   *
   * @var string
   */
  public $response = '';

  /**
   * Contains backend options.
   *
   * @var array
   */
  public $options = array();

  /**
   * Get backend type.
   *
   * @return string
   *    Backend type.
   */
  public function getType() {
    return isset($this->type) ? $this->type : '';
  }

  /**
   * Set backend type.
   *
   * @param string $type
   *    Backend type.
   */
  public function setType($type) {
    $this->type = $type;
  }

  /**
   * Get formatter handler name.
   *
   * @return string
   *    Formatter handler name.
   */
  public function getFormatter() {
    return isset($this->formatter) ? $this->formatter : '';
  }

  /**
   * Set formatter handler name.
   *
   * @param string $formatter
   *    Formatter handler name.
   */
  public function setFormatter($formatter) {
    $this->formatter = $formatter;
  }

  /**
   * Get response handler name.
   *
   * @return string
   *    Response handler name.
   */
  public function getResponse() {
    return $this->response;
  }

  /**
   * Set response handler name.
   *
   * @param string $response
   *    Response handler name.
   */
  public function setResponse($response) {
    $this->response = $response;
  }

  /**
   * {@inheritdoc}
   */
  public function form(array &$form, array &$form_state, $op) {
    parent::form($form, $form_state, $op);
    $plugin = PluginManager::getInstance('backend');

    $form['type'] = $plugin->getFormRadios(t('Backend type'), $this->getType(), TRUE);

    $form['component'] = array(
      '#type' => 'vertical_tabs',
      '#tree' => FALSE,
    );
    foreach ($plugin->getComponents() as $component) {

      $plugin->setComponent($component);
      $label = $plugin->getComponentLabel($component);

      $form["component_$component"] = array(
        '#type' => 'fieldset',
        '#title' => $label,
        '#collapsible' => TRUE,
        '#group' => 'component',
      );
      $form["component_$component"][$component] = $plugin->getFormRadios($label, '', TRUE);

      foreach ($plugin->getInfo() as $type => $info) {
        if ($plugin->isComponentConfigurable($type)) {
          $element = array(
            '#type' => 'fieldset',
            '#title' => t('@component options', array('@component' => $label)),
            '#collapsible' => TRUE,
            '#group' => "component_$component",
          );

          // @todo: make a proper component configuration factory for this.
          $class = $plugin->getComponentConfigurationClass($type);

          /** @var AbstractComponentConfiguration $component_configuration */
          $component_configuration = new $class($this);
          $component_configuration->form($element, $form_state, $op);

          $form["component_$component"]["{$component}_configuration"] = $element;
        }
      }
    }
  }

}
