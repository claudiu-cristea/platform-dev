<?php
/**
 * @file
 * Webservice definition and behaviour.
 */

/**
 * Implements hook_menu().
 */
function tmgmt_poetry_menu() {
  $items = array();
  $items['tmgmt_poetry/service_callback'] = array(
    'page callback' => '_tmgmt_poetry_service_callback',
    'access callback' => TRUE,
  );
  $items[drupal_get_path("module", "tmgmt_poetry") . "/wsdl/PoetryIntegration.wsdl"] = array(
    'page callback' => '_tmgmt_poetry_wsdl',
    'access callback' => TRUE,
  );
  return $items;
}


/**
 * Function to define our webservice and register the available methods.
 */
function _tmgmt_poetry_service_callback() {
  // Generate our own SOAP server.
  $context = stream_context_create();
  $uri = url(drupal_get_path("module", "tmgmt_poetry") . "/wsdl/PoetryIntegration.wsdl", array('absolute' => TRUE, 'language' => (object) array('language' => FALSE)));

  $server = new SoapServer($uri, array(
    'stream_context' => $context,
    'cache_wsdl' => WSDL_CACHE_NONE,
  ));
  $server->AddFunction("FPFISPoetryIntegrationRequest");
  $server->handle();
}

/**
 * Helper function to generate an answer xml for received Poetry requests.
 */
function _tmgmt_poetry_generate_answer_xml($job = NULL, $message = 'OK', $code = 0, $received_request = NULL) {
  // Create initial XML element using POETRY headers.
  $xml = new SimpleXMLElement("<?xml version=\"1.0\" encoding=\"utf-8\" ?>
    <POETRY xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"
    xsi:noNamespaceSchemaLocation=\"http://intragate.ec.europa.eu/DGT/poetry_services/poetry.xsd\">
    </POETRY>");

  // Add main request element.
  $request = $xml->addChild('request');

  if ($received_request !== NULL) {
    $reference = implode('/', (array) $received_request->demandeId);
  }
  elseif ($job !== NULL) {
    $reference = $job->reference;
  }

  $request->addAttribute('id', $reference);
  $request->addAttribute('type', 'status');

  // Add request identifier.
  $demande_id_data = explode('/', $reference);
  $demande_id = $request->addChild('demandeId');
  $demande_id->addChild('codeDemandeur', $demande_id_data[0]);
  $demande_id->addChild('annee', $demande_id_data[1]);
  $demande_id->addChild('numero', $demande_id_data[2]);
  $demande_id->addChild('version', $demande_id_data[3]);
  $demande_id->addChild('partie', $demande_id_data[4]);
  $demande_id->addChild('produit', $demande_id_data[5]);

  $status = $request->addChild('status');
  $status->addAttribute('type', 'request');
  $status->addAttribute('code', $code);

  $status_date = $status->addChild('statusDate', date('d/m/Y'));
  $status_date->addAttribute('format', 'dd/mm/yyyy');

  $status_time = $status->addChild('statusTime', date('h:i:s'));
  $status_time->addAttribute('format', 'hh:mm:ss');

  $status->addChild('statusMessage', $message);

  return $xml;
}

/**
 * Helper function to authenticate request made to our service.
 */
function _tmgmt_poetry_authenticate($user, $password) {
  // Get Poetry configuration.
  $poetry = variable_get("poetry_service");
  return ($poetry['callback_user'] === $user && $poetry['callback_password'] === $password);
}

/**
 * Function available to call from our webservice.
 */
function FPFISPoetryIntegrationRequest($user, $password, $msg) {

  watchdog("POETRY", "Poetry accessed callback");
  watchdog('tmgmt_poetry: translation response', $msg, array(), WATCHDOG_DEBUG);

  // Authenticate the request.
  if (_tmgmt_poetry_authenticate($user, $password)) {
    $languages = language_list();
    $poetry_translator = tmgmt_translator_load('poetry');
    // Load the received XML and load the referenced Job.
    $xml = simplexml_load_string($msg);
    $request = $xml->request;
    $reference = implode("/", (array) $request->demandeId);

    // Get main job in order to register the messages.
    $main_reference = 'MAIN_%_POETRY_%' . $reference;
    $language_job = array();
    $ids = _tmgmt_poetry_obtain_related_translation_jobs($language_job, $main_reference)->fetchAll();
    $ids = array_shift($ids);
    $message_job = tmgmt_job_load($ids->tjid);

    // If the received message has a status, record it in the job.
    if (isset($request->status)) {
      $cancelled = FALSE;
      foreach ($request->status as $status) {

        // Check status code.
        switch ($status['code']) {
          case 'SUS':
            $status_message = t('Suspended');
            $cancelled = FALSE;
            break;

          case 'ONG':
            $status_message = 'Ongoing';
            $cancelled = FALSE;
            break;

          case 'LCK':
            $status_message = 'Acceptance in Progress';
            $cancelled = FALSE;
            break;

          case 'EXE':
            $status_message = 'Executed';
            $cancelled = FALSE;
            break;

          case 'REF':
            $status_message = 'Refused';
            $cancelled = TRUE;
            break;

          case 'CNL':
            $status_message = 'Canceled';
            $cancelled = TRUE;
            break;
        }

        // Status update for the whole request.
        if ($status['type'] == 'demande') {
          if (isset($status->statusMessage)) {
            $message = (string) $status->statusMessage;
          }
          else {
            $message = t('No message.');
          }

          $message_job->addMessage("DGT update received. Request status: @status. Message: @message", array('@status' => $status_message, '@message' => $message));
          if ($cancelled) {
            $reference = '%' . $reference;
            $ids = _tmgmt_poetry_obtain_related_translation_jobs(array(), $reference)->fetchAll();
            foreach ($ids as $id) {
              $job = tmgmt_job_load($id->tjid);
              $job->aborted("Request aborted by DGT.", array());
            }
          }
          elseif ($message_job->isAborted()) {
            $reference = '%' . $reference;
            $ids = _tmgmt_poetry_obtain_related_translation_jobs(array(), $reference)->fetchAll();
            foreach ($ids as $id) {
              $reopen_job = tmgmt_job_load($id->tjid);
              $reopen_job->setState(TMGMT_JOB_STATE_ACTIVE, t('DGT reopened the request'));
              $reopen_job_item = tmgmt_job_item_load($ids->tjiid);
              $reopen_job_item->active();
            }
          }
        }

        // Status update for a specific language.
        if ($status['type'] == 'attribution') {
          if (isset($status['lgCode']) && !empty($status['lgCode'])) {
            $reference = '%' . $reference;
            $language_job = array(strtolower((string) $status['lgCode']));
            $lanaguage_job = $poetry_translator->mapToLocalLanguage($language_job);
          }
          else {
            $reference = 'MAIN_%_POETRY_%' . $reference;
            $language_job = array();
          }
          $ids = _tmgmt_poetry_obtain_related_translation_jobs($language_job, $reference)->fetchAll();
          $ids = array_shift($ids);
          $job = tmgmt_job_load($ids->tjid);
          $job_item = tmgmt_job_item_load($ids->tjiid);
          $language = $languages[$job->target_language]->name;
          $message_job->addMessage("DGT update received. Affected language: @language. Request status: @status.", array('@language' => $language, '@status' => $status_message));
          db_update('poetry_status')
              ->fields(array('status' => $status_message))
              ->condition('tjiid', $job_item->tjiid, '=')
              ->execute();
        }
      }
    }

    // Check the attributions to look for translations and delai confirmations.
    foreach ($request->attributions as $attributions) {
      $reference = '%' . $reference;
      $language_job = array(strtolower((string) $attributions['lgCode']));
      $language_job = $poetry_translator->mapToLocalLanguage($language_job);
      $ids = _tmgmt_poetry_obtain_related_translation_jobs($language_job, $reference)->fetchAll();
      $ids = array_shift($ids);
      $job = tmgmt_job_load($ids->tjid);
      $job_item = tmgmt_job_item_load($ids->tjiid);

      if (!isset($attributions['format']) || (isset($attributions['format']) && (strpos($job->getSetting('export_format'), strtolower((string) $attributions['format'])) === FALSE))) {
        $message_job->addMessage('Received format "@format" is not compatible, translation job format "@job_format" should be used instead', array('@format' => (string) $attributions['format'], '@job_format' => $job->getSetting('export_format')));
        $xml = _tmgmt_poetry_generate_answer_xml($message_job, 'ERROR: Received format is not compatible', -1, $request);
        return $xml->asXML();
      }

      $language = $languages[$job->target_language]->name;

      // Update the delai provided by DGT.
      if (isset($attributions->attributionsDelaiAccepted)) {
        db_update('poetry_status')
          ->fields(array('delai' => (string) $attributions->attributionsDelaiAccepted))
          ->condition('tjiid', $job_item->tjiid, '=')
          ->execute();
      }

      // If the received message has the translated file, add it to the job.
      if (isset($attributions->attributionsFile)) {
        $content = (string) $request->attributions->attributionsFile;
        $content = _tmgmt_poetry_replace_job_in_content(base64_decode($content), $job, $job_item);

        $controller = tmgmt_file_format_controller($job->getSetting('export_format'));
        if ($controller) {
          // Validate the file.
          $validated_job = $controller->validateImport($content);
          if (!$validated_job) {
            $message_job->addMessage('@language Failed to validate file, import aborted.', array('@language' => $language), 'error');
          }
          elseif ($validated_job->tjid != $job->tjid) {
            $uri = $validated_job->uri();
            $label = $validated_job->label();
            $message_job->addMessage('@language Import file is from job <a href="@url">@label</a>, import aborted.', array(
              '@language' => $language,
              '@url' => url($uri['path']),
              '@label' => $label,
            ));
          }
          else {
            try {
              if (!$job->isAborted()) {
                // Validation successful, start import.
                $job->addTranslatedData($controller->import($content));
                $message_job->addMessage('@language Successfully received the translation file.', array('@language' => $language));
                // Update the status to executed when we receive a translation.
                db_update('poetry_status')
                  ->fields(array('status' => 'Executed'))
                  ->condition('tjiid', $job_item->tjiid, '=')
                  ->execute();
              }
              // Save the file and make it available in the job.
              $name = "JobID" . $job->tjid . '_' . $job->source_language . '_' . $job->target_language;
              $path = 'public://tmgmt_file/' . $name . '.' . $job->getSetting('export_format');
              $dirname = dirname($path);
              if (file_prepare_directory($dirname, FILE_CREATE_DIRECTORY)) {
                $file = file_save_data($content, $path);
                file_usage_add($file, 'tmgmt_file', 'tmgmt_job', $job->tjid);
                $message_job->addMessage('Received tanslation can be downloaded <a href="!link">here</a>.', array('!link' => file_create_url($path)));
              }
            }
            catch (Exception $e) {
              $message_job->addMessage('@language File import failed with the following message: @message', array('@language' => $language, '@message' => $e->getMessage()), 'error');
            }
          }
        }
      }
    }

    // Send answer to poetry.
    $xml = _tmgmt_poetry_generate_answer_xml($job, 'OK', 0, $request);
    return $xml->asXML();
  }
  else {
    watchdog("POETRY", "Poetry callback failed authentication.");
    $xml = simplexml_load_string($msg);
    $request = $xml->request;
    // Send answer to poetry.
    $xml = _tmgmt_poetry_generate_answer_xml(NULL, 'ERROR: Failed authentication', -1, $request);
    return $xml->asXML();
  }
}

/**
 * Replace job id in received content.
 */
function _tmgmt_poetry_replace_job_in_content($content, $job, $job_item) {
  switch ($job->getSetting('export_format')) {
    case 'html_poetry':
    case 'xml_dgt':
      $dom = new DOMDocument();
      if (!$dom->loadHTML($content)) {
        return FALSE;
      }
      $xml = simplexml_import_dom($dom);

      if (count($xml->head->meta) > 0) {
        foreach ($xml->head->meta as $meta_tag) {
          if (isset($meta_tag['name'])) {
            switch ($meta_tag['name']) {
              case 'JobID':
                $meta_tag['content'] = $job->tjid;
                break;

              case 'languageSource':
                $meta_tag['content'] = $job->getTranslator()->mapToRemoteLanguage($job->source_language);
                break;

              case 'languageTarget':
                $meta_tag['content'] = $job->getTranslator()->mapToRemoteLanguage($job->target_language);
                break;
            }
          }
        }
      }
      if (isset($xml->head->title)) {
        $xml->head->title = "Job ID " . $job->tjid;
      }
      foreach ($xml->body->div as $parent_div) {
        if ($parent_div['class'] == 'meta' && $parent_div['id'] == 'languageTarget') {
          $parent_div[0] = $job->target_language;
        }
        if ($parent_div['class'] == 'asset') {
          $parent_div['id'] = $job_item->tjiid;

          /** @var SimpleXMLElement $div */
          foreach ($parent_div->div as $div) {
            if ($div['class'] == 'atom') {
              $data = substr($div['id'], 1);
              $data = base64_decode(str_pad(strtr($data, '-_', '+/'), strlen($data) % 4, '=', STR_PAD_RIGHT));
              $data = explode(']', $data);
              $data[0] = $job_item->tjiid;
              $data = implode(']', $data);
              $div['id'] = 'b' . rtrim(strtr(base64_encode($data), '+/', '-_'), '=');
            }
          }
        }
      }
      $dom->loadXML($xml->asXML());
      $result = $dom->saveHTML();
      break;

    case 'xliff_xsl_xml_poetry':
      $xml = simplexml_load_string($content);

      // Register the XLIFF namespace, required for xpath.
      $xml->registerXPathNamespace('xliff', 'urn:oasis:names:tc:xliff:document:1.2');
      // Check if the job has a valid job reference.
      $phase = $xml->xpath("//xliff:phase[@phase-name='extraction']");
      $phase = reset($phase);
      $phase['job-id'] = $job->tjid;
      $xml->file['target-language'] = $job->target_language;

      $xml->file->body->group['id'] = $job_item->tjiid;

      /** @var SimpleXMLElement $transunit */
      foreach ($xml->file->body->group->{'trans-unit'} as $transunit) {
        $data = explode(']', $transunit['id']);
        $old_tjiid = $data[0];
        $data[0] = $job_item->tjiid;
        $data = implode(']', $data);
        $transunit['id'] = $data;
        $transunit['resname'] = $data;

        $target = dom_import_simplexml($transunit->target);
        $target->setAttribute('xml:lang', $job->target_language);

        /** @var SimpleXMLElement $transunit */
        foreach ($transunit->source->children() as $tag) {
          $tag['id'] = str_replace('tjiid' . $old_tjiid . '-', 'tjiid' . $job_item->tjiid . '-', $tag['id']);
        }
      }
      $result = $xml->saveXML();
      break;
  }
  return $result;
}

/**
 * Function to return our webservice description.
 */
function _tmgmt_poetry_wsdl() {
  $uri = url('tmgmt_poetry/service_callback', array('absolute' => TRUE, 'language' => (object) array('language' => FALSE)));
  $wsdl = '<?xml version="1.0"?>
<definitions name="FPFISPoetryIntegration"
             targetNamespace="urn:FPFISPoetryIntegration"
             xmlns:tns="urn:FPFISPoetryIntegration"
             xmlns:xsd="http://www.w3.org/2001/XMLSchema"
             xmlns:soap="http://schemas.xmlsoap.org/wsdl/soap/"
             xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"
             xmlns:wsdl="http://schemas.xmlsoap.org/wsdl/"
             xmlns="http://schemas.xmlsoap.org/wsdl/">

  <message name="FPFISPoetryIntegrationMessage">
    <part name="user" type="xsd:string" />
    <part name="password" type="xsd:string" />
    <part name="msg" type="xsd:string" />
  </message>

  <message name="FPFISPoetryIntegrationResponse">
    <part name="return" type="xsd:string" />
  </message>

  <portType name="FPFISPoetryIntegrationPort">
    <operation name="FPFISPoetryIntegrationRequest">
      <input message="tns:FPFISPoetryIntegrationMessage" />
      <output message="tns:FPFISPoetryIntegrationResponse" />
    </operation>
  </portType>

  <binding name="FPFISPoetryIntegrationBinding" type="tns:FPFISPoetryIntegrationPort">
    <soap:binding style="rpc" transport="http://schemas.xmlsoap.org/soap/http" />
    <operation name="FPFISPoetryIntegrationRequest">
      <soap:operation soapAction="urn:FPFISPoetryIntegration#FPFISPoetryIntegrationRequest" />
      <input>
      <soap:body use="encoded" namespace="urn:FPFISPoetryIntegration"
                 encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" />
      </input>
      <output>
        <soap:body use="encoded" namespace="urn:FPFISPoetryIntegration"
                   encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" />
      </output>
    </operation>
  </binding>

  <service name="FPFISPoetryIntegrationServicio">
    <port name="FPFISPoetryIntegrationPort" binding="tns:FPFISPoetryIntegrationBinding">
      <soap:address location="' . $uri . '" />
    </port>
  </service>
</definitions>';
  drupal_add_http_header('Content-Type', 'application/xml; utf-8');
  print $wsdl;
  exit(0);
}

/**
 * Local Exception class.
 */
class TMGMTPoetryException extends Exception {
  /**
   * Override of the parent __construct method.
   */
  public function __construct($message = "", $data = array(), $code = 0, Exception $previous = NULL) {
    parent::__construct(strtr($message, $data), $code, $previous);
  }

}
