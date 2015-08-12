<?php

/**
 * @file
 * Contains Drupal\nexteuropa_integration\Consumer\Consumer.
 */

namespace Drupal\nexteuropa_integration\Consumer;

use Drupal\nexteuropa_integration\Backend\BackendFactory;
use Drupal\nexteuropa_integration\Backend\BackendInterface;
use Drupal\nexteuropa_integration\Configuration\AbstractConfiguration;
use Drupal\nexteuropa_integration\Configuration\ConfigurableInterface;
use Drupal\nexteuropa_integration\Configuration\ConfigurationFactory;
use Drupal\nexteuropa_integration\Consumer\Configuration\ConsumerConfiguration;
use Drupal\nexteuropa_integration\Consumer\Migrate\AbstractMigration;
use Drupal\nexteuropa_integration\Consumer\Migrate\MigrateItemJSON;
use Drupal\nexteuropa_integration\Consumer\Migrate\MigrateListJSON;
use Drupal\nexteuropa_integration\Consumer\MappingHandler\AbstractMappingHandler;

/**
 * Interface ConsumerInterface.
 *
 * @package Drupal\nexteuropa_integration\Consumer
 */
class Consumer extends AbstractMigration implements ConsumerInterface, ConfigurableInterface {

  /**
   * List supported entity destinations so far. To be expanded soon.
   *
   * @var array
   */
  protected $supportedDestinations = array(
    'node' => '\MigrateDestinationNode',
    'taxonomy_term' => '\MigrateDestinationTerm',
  );

  /**
   * Configuration object.
   *
   * @var ConsumerConfiguration
   */
  protected $configuration;

  /**
   * Backend object.
   *
   * @var BackendInterface
   */
  protected $backend;

  /**
   * {@inheritdoc}
   */
  public function __construct(array $arguments) {

    self::validateArguments($arguments);
    parent::__construct($arguments);

    /** @var ConsumerConfiguration $configuration */
    $configuration = ConfigurationFactory::load('integration_consumer', $arguments['consumer']['configuration']);
    $this->setConfiguration($configuration);

    $this->setMap($this->getMapInstance());
    $this->setDestination($this->getDestinationInstance());

    // Mapping default language is necessary for correct translation handling.
    $this->addFieldMapping('language', 'default_language');

    // @todo: Make the following an option set via UI.
    $this->addFieldMapping('promote')->defaultValue(FALSE);
    $this->addFieldMapping('status')->defaultValue(NODE_NOT_PUBLISHED);

    // Apply mapping.
    foreach ($this->getConfiguration()->getMapping() as $destination => $source) {
      $this->addFieldMapping($destination, $source);
      $this->processMappingHandlers($destination, $source);
    }

    $backend = BackendFactory::getInstance($this->getConfiguration()->getBackend());
    $this->setSource(new \MigrateSourceList(
      new MigrateListJSON($backend->getListUri()),
      new MigrateItemJSON($backend->getResourceUri(), array()),
      array()
    ));
  }

  /**
   * {@inheritdoc}
   */
  public function getSourceKey() {
    return array(
      '_id' => array(
        'type' => 'varchar',
        'length' => 255,
        'not null' => TRUE,
      ),
    );
  }

  /**
   * {@inheritdoc}
   */
  public function getConfiguration() {
    return $this->configuration;
  }

  /**
   * {@inheritdoc}
   */
  public function setConfiguration(AbstractConfiguration $configuration) {
    $this->configuration = $configuration;
  }

  /**
   * {@inheritdoc}
   */
  public static function register($name) {
    $configuration = ConfigurationFactory::load('integration_consumer', $name);

    $arguments = array();
    $arguments['consumer']['configuration'] = $configuration->getMachineName();

    self::validateArguments($arguments);
    \Migration::registerMigration(__CLASS__, $configuration->getMachineName(), $arguments);
  }

  /**
   * {@inheritdoc}
   */
  static public function getInstance($machine_name) {
    self::register($machine_name);
    return parent::getInstance($machine_name);
  }

  /**
   * Process field mapping handlers.
   *
   * @param string $destination_field
   *    Destination field name.
   * @param string|null $source_field
   *    Source field name.
   */
  protected function processMappingHandlers($destination_field, $source_field = NULL) {

    $handlers = nexteuropa_integration_producer_get_consumer_mapping_handler_info();
    foreach ($handlers as $name => $info) {
      /** @var AbstractMappingHandler $handler */
      $handler = new $info['class']($this);
      $handler->process($destination_field, $source_field);
    }
  }

  /**
   * Get map object instance depending on entity type setting.
   *
   * @return \MigrateMap
   *    Map object instance.
   */
  protected function getMapInstance() {
    /** @var \MigrateDestinationNode $destination_class */
    $destination_class = $this->getDestinationClass();
    return new \MigrateSQLMap($this->getMachineName(), $this->getSourceKey(), $destination_class::getKeySchema());
  }

  /**
   * Get destination object instance depending on entity type setting.
   *
   * @return \MigrateDestination
   *    Destination object instance.
   */
  protected function getDestinationInstance() {
    $destination_class = $this->getDestinationClass();
    $bundle = $this->getConfiguration()->getEntityBundle();
    return new $destination_class($bundle);
  }

  /**
   * Return migration destination class depending on entity type setting.
   *
   * @return string
   *    Destination class name.
   */
  protected function getDestinationClass() {
    $entity_type = $this->getConfiguration()->getEntityType();
    if (isset($this->supportedDestinations[$entity_type])) {
      return $this->supportedDestinations[$entity_type];
    }
    throw new \InvalidArgumentException("Entity destination $entity_type not supported.");
  }

  /**
   * Make sure required arguments are present and valid.
   *
   * @param array $arguments
   *    Constructor's $arguments array.
   */
  static private function validateArguments(array $arguments) {

    if (!isset($arguments['consumer'])) {
      throw new \InvalidArgumentException(t('Consumer argument missing: "consumer".'));
    }
    if (!isset($arguments['consumer']['configuration'])) {
      throw new \InvalidArgumentException('Consumer sub-argument missing: "configuration".');
    }
  }

}