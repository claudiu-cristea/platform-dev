@api
Feature: Multilingual features
  In order to easily understand the content of the European Commission
  As a citizen of the European Union
  I want to be able to read content in my native language


  Background:
    Given the following languages are available:
      | languages |
      | en        |
      | fr        |
      | de        |

  Scenario: Content can be translated in available languages
    Given I am viewing a multilingual "page" content:
      | language | title                        |
      | en       | This title is in English     |
      | fr       | Ce titre est en Français     |
      | de       | Dieser Titel ist auf Deutsch |
    Then I should see the heading "This title is in English"
    And I click "English" in the "header_top" region
    Then I should be on the language selector page
    And I click "Français"
    Then I should see the heading "Ce titre est en Français"
    And I click "Français" in the "header_top" region
    Then I should be on the language selector page
    When I click "Deutsch"
    And I should see the heading "Dieser Titel ist auf Deutsch"
    And I click "Deutsch"
    Then I should be on the language selector page
    And I click "English"
    Then I should see the heading "This title is in English"

  Scenario: Custom URL suffix language negotiation is applied by default on new content.
    Given I am logged in as a user with the 'administrator' role
    And I am viewing a multilingual "page" content:
      | language | title            |
      | en       | Title in English |
      | fr       | Title in French  |
      | de       | Title in German  |
    Then I should be on "content/title-english_en"
    And I click "English" in the "header_top" region
    Then I should be on the language selector page
    And I click "Français"
    Then I should be on "content/title-english_fr"
    And I click "Français" in the "header_top" region
    Then I should be on the language selector page
    And I click "Deutsch"
    Then I should be on "content/title-english_de"

  Scenario: Enable multiple languages
    Given I am logged in as a user with the 'administrator' role
    When I go to "admin/config/regional/language"
    Then I should see "English"
    And I should see "French"
    And I should see "German"

  @cleanEnvironment
  Scenario: Enable language suffix and check the base path
    Given I am logged in as a user with the 'administrator' role
    When I go to "admin/config/regional/language/configure"
    And I check the box "edit-language-enabled-nexteuropa-multilingual-url-suffix"
    And I uncheck the box "edit-language-enabled-locale-url"
    And I press the "Save settings" button
    Then I should see the success message "Language negotiation configuration saved."
    When I go to "admin/config/regional/language/edit/en"
    And I fill in "edit-prefix" with "en-prefix"
    And I press the "Save language" button
    And I go to "admin/config/system/site-information"
    And I fill in "edit-site-frontpage" with "admin/fake-url"
    And I select "01000" from "edit-classification"
    And I press the "Save configuration" button
    Then I should see the success message "The configuration options have been saved."
    And the cache has been cleared
    And I should not see "admin/fake-url" in the ".form-item-site-frontpage span.field-prefix" element
    And I should not see "en-prefix" in the ".form-item-site-frontpage span.field-prefix" element
    When I go to "admin/config/regional/language/edit/en"
    And I fill in "edit-prefix" with "en"
    And I press the "Save language" button

  Scenario: Path aliases are not deleted when translating content via translation management
    Given local translator "Translator A" is available
    Given I am logged in as a user with the "administrator" role
    Given I am viewing a multilingual "page" content:
      | language | title                        |
      | en       | This title is in English     |
    And I click "Translate" in the "primary_tabs" region
    And I select the radio button "" with the id "edit-languages-de"
    And I press the "Request translation" button
    And I select "Translator A" from "Translator"
    And I press the "Submit to translator" button
    Then I should see the following success messages:
      | success messages                        |
      | The translation job has been submitted. |
    And I click "Translation"
    Then I should see "This title is in English"
    And I click "manage" in the "This title is in English" row
    And I click "view" in the "In progress" row
    And I fill in "Translation" with "Dieser Titel ist auf Deutsch"
    And I press the "Save" button
    And I click "reviewed" in the "The translation of This title is in English to German is finished and can now be reviewed." row
    And I press the "Save as completed" button
    Then I should see "The translation for This title is in English has been accepted."
    And I click "This title is in English"
    And I should be on "content/title-english_en"
    And I should see the heading "This title is in English"
    And I visit "content/title-english_de"
    And I should see the heading "Dieser Titel ist auf Deutsch"