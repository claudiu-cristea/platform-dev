<?php
/**
 * @file
 * Provides administration functionality for the map entity.
 */

/**
 * Add new map page callback.
 */
function ne_map_add() {
  $map = entity_create('ne_map', array('type' => 'ne_map'));
  drupal_set_title(t('Create map'));
  $output = drupal_get_form('ne_map_form', $map);
  return $output;
}

/**
 * Constructs the map form.
 */
function ne_map_form($form, &$form_state, $map) {
  $form_state['map'] = $map;

  // Provides a title field.
  $form['title'] = array(
    '#type' => 'textfield',
    '#required' => TRUE,
    '#title' => t('Name'),
    '#default_value' => $map->title,
  );

  // Provides a description field.
  $form['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Description'),
    '#default_value' => $map->description,
  );

  // Stores the user ID.
  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $map->uid,
  );

  // Provides a field to store various map settings.
  $settings = new NEMapSettings();
  if (!empty($map->settings)) {
    $settings->customize(drupal_json_decode($map->settings));
  }

  // @todo. how to prevent this?
  $settings = $settings->settings;

  // @todo decide on availibility of certain fields.
  // Provides the height field whose values are stored as json in the settings.
  $form['height'] = ne_map_form_elements('height', $settings);

  // Provides the map center field whose values is stored as json in the
  // settings.
  $form['center'] = ne_map_form_elements('center', $settings);

  // Provides the pop-up fields whose values is stored as json in the settings.
  $form['popup'] = ne_map_form_elements('popup', $settings);

  // Provides the zoom fields whose values is stored as json in the settings.
  $form['zoom'] = ne_map_form_elements('zoom', $settings);
  $form['#validate'][] = 'ne_map_form_validate';

  // Provides the tiles field whose values is stored as json in the settings.
  $form['tiles'] = ne_map_form_elements('tiles', $settings);

  // Provides the attribution fields whose values is stored as json in the
  // settings.
  $form['attribution'] = ne_map_form_elements('attribution', $settings);

  // Provides the clustering field whose values is stored as json in the
  // settings.
  $form['clustering'] = ne_map_form_elements('clustering', $settings);

  // Provides a checkbox to show /hide settings field.
  $form['show_settings'] = array(
    '#type' => 'checkbox',
    '#title' => t('Show settings'),
    '#default_value' => $settings['show_settings'],
    '#weight' => 98,
  );
  $form['settings'] = array(
    '#weight' => 99,
    '#title' => t('Settings'),
    '#type' => 'textarea',
    '#default_value' => isset($map->settings) ? $map->settings : '',
    '#description' => t('Settings (json encoded)'),
    '#states' => array(
      'visible' => array(
        ':input[name="show_settings"]' => array('checked' => TRUE),
      ),
    ),
  );

  field_attach_form('ne_map', $map, $form, $form_state);
  $submit = array();
  if (!empty($form['#submit'])) {
    $submit += $form['#submit'];
  }
  $form['actions'] = array(
    '#weight' => 100,
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save map'),
    '#submit' => $submit + array('ne_map_form_submit'),
  );

  // Adds delete button when editing a map.
  $map_id = entity_id('ne_map', $map);
  if (!empty($map_id) && ne_map_access('edit', $map)) {
    $form['actions']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#submit' => array('ne_map_form_submit_delete'),
    );
  }
  $form['#validate'][] = 'ne_map_form_validate';
  return $form;
}

/**
 * Provides validation for the map form.
 */
function ne_map_form_validate($form, &$form_state) {
  // @todo. Add validation.
}

/**
 * Handles map form submission.
 */
function ne_map_form_submit($form, &$form_state) {
  $map = $form_state['map'];
  entity_form_submit_build_entity('ne_map', $map, $form, $form_state);
  map_save($map);
  $map_uri = entity_uri('ne_map', $map);
  $form_state['redirect'] = $map_uri['path'];
  drupal_set_message(t('Map %title saved.', array('%title' => entity_label('ne_map', $map))));
}

/**
 * Handles map delete form submission.
 */
function ne_map_form_submit_delete($form, &$form_state) {
  $map = $form_state['map'];
  $map_uri = entity_uri('ne_map', $map);
  $form_state['redirect'] = $map_uri['path'] . '/delete';
}

/**
 * Delete confirmation form.
 */
function ne_map_delete_form($form, &$form_state, $map) {
  $form_state['map'] = $map;

  // Always provide entity id in the same form key as in the entity edit form.
  $form['map_type_id'] = array(
    '#type' => 'value',
    '#value' => entity_id('ne_map', $map),
  );
  $map_uri = entity_uri('ne_map', $map);
  return confirm_form($form,
    t('Are you sure you want to delete map %title?', array('%title' => entity_label('ne_map', $map))),
    $map_uri['path'],
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Delete form submit handler.
 */
function ne_map_delete_form_submit($form, &$form_state) {
  $map = $form_state['map'];
  ne_map_delete($map);
  drupal_set_message(t('Map %title deleted.', array('%title' => entity_label('ne_map', $map))));
  $form_state['redirect'] = '<front>';
}