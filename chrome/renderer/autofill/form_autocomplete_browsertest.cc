// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/test_utils/autofill_form_test_utils.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

namespace {

using ::base::test::RunOnceClosure;
using ::blink::WebDocument;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebInputElement;
using ::blink::WebString;
using ::testing::_;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::ElementsAre;
using ::testing::Property;

class FormAutocompleteTest : public test::AutofillRendererTest {
 public:
  FormAutocompleteTest() = default;
  FormAutocompleteTest(const FormAutocompleteTest&) = delete;
  FormAutocompleteTest& operator=(const FormAutocompleteTest&) = delete;
  ~FormAutocompleteTest() override = default;

 protected:
  void SetUp() override {
    AutofillRendererTest::SetUp();

    focus_test_utils_ = std::make_unique<test::FocusTestUtils>(
        base::BindRepeating(&FormAutocompleteTest::ExecuteJavaScriptForTests,
                            base::Unretained(this)));
  }

  // See `AutofillRendererTest::SimulateFillForm`. Extracts `FormData` from main
  // frame if none is provided.
  AssertionResult SimulateFillForm(
      std::optional<FormData> form_data = std::nullopt) {
    if (!form_data) {
      form_data = ExtractFormData("myForm");
    }
    if (!form_data) {
      return AssertionFailure();
    }
    return AutofillRendererTest::SimulateFillForm(
        *form_data, "fname", {{u"fname", u"John"}, {u"lname", u"Smith"}});
  }

  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<test::FocusTestUtils> focus_test_utils_;
};

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form
// having non-fillable fields.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventsAfterAutofill) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "<label>Middle Name:</label><input id='mname' name='2'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  ASSERT_TRUE(SimulateFillForm());

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element.
TEST_F(FormAutocompleteTest,
       VerifyFocusAndBlurEventsAfterAutofillWithFocusedElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  ASSERT_TRUE(SimulateFillForm());

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form having
// single field.
TEST_F(FormAutocompleteTest,
       VerifyFocusAndBlurEventAfterAutofillWithFocusedElementForSingleElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  ASSERT_TRUE(SimulateFillForm());

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Tests that a field is added to the form between the times of triggering
// and executing the filling.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventAfterElementAdded) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  std::optional<FormData> form = ExtractFormData("myForm");
  ASSERT_TRUE(form);
  // Simulate that the form was modified between parsing and executing the fill.
  // The element is inserted at the beginning of the form to verify that
  // everything works correctly even if `renderer_id`s of the `<input>`
  // elements are not in ascending order.
  ExecuteJavaScriptForTests(
      "document.getElementById('fname').insertAdjacentHTML('beforebegin', "
      "'<label>Zip code:</label><input id=\"zip_code\"/>');");
  ASSERT_TRUE(SimulateFillForm(form));

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that a field is removed from the form between the times of
// triggering and executing the filling.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventAfterElementRemoved) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  std::optional<FormData> form = ExtractFormData("myForm");
  ASSERT_TRUE(form);

  ExecuteJavaScriptForTests("document.getElementById('lname').remove()");
  ASSERT_TRUE(SimulateFillForm(form));

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Unit test for AutofillAgent::AcceptDataListSuggestion.
TEST_F(FormAutocompleteTest, AcceptDataListSuggestion) {
  LoadHTML(
      "<html>"
      "<input id='empty' type='email' multiple />"
      "<input id='multi_one' type='email' multiple value='one@example.com'/>"
      "<input id='multi_two' type='email' multiple"
      "  value='one@example.com,two@example.com'/>"
      "<input id='multi_trailing' type='email' multiple"
      "  value='one@example.com,two@example.com,'/>"
      "<input id='not_multi' type='email'"
      "  value='one@example.com,two@example.com,'/>"
      "<input id='not_email' type='text' multiple"
      "  value='one@example.com,two@example.com,'/>"
      "</html>");
  WebDocument document = GetMainFrame()->GetDocument();

  // Each case tests a different field value with the same suggestion.
  const std::u16string kSuggestion = u"suggestion@example.com";
  struct TestCase {
    std::string id;
    std::string expected;
  } cases[] = {
      // Empty text field; expect to populate with suggestion.
      {"empty", "suggestion@example.com"},
      // Single entry; expect to replace with suggestion.
      {"multi_one", "suggestion@example.com"},
      // Two comma-separated entries; expect to replace second with suggestion.
      {"multi_two", "one@example.com,suggestion@example.com"},
      // Two comma-separated entries with trailing comma; expect to append
      // suggestion.
      {"multi_trailing",
       "one@example.com,two@example.com,suggestion@example.com"},
      // Do not apply this logic for a non-multiple or non-email field.
      {"not_multi", "suggestion@example.com"},
      {"not_email", "suggestion@example.com"},
  };

  for (const auto& c : cases) {
    ASSERT_TRUE(SimulateElementClick(c.id));

    WebElement element = document.GetElementById(WebString::FromUtf8(c.id));
    ASSERT_TRUE(element);
    WebInputElement input_element = element.To<WebInputElement>();

    autofill_agent().AcceptDataListSuggestion(
        form_util::GetFieldRendererId(input_element), kSuggestion);
    EXPECT_EQ(c.expected, input_element.Value().Utf8()) << "Case id: " << c.id;
  }
}

TEST_F(FormAutocompleteTest, SelectControlChanged) {
  LoadHTML(
      "<html>"
      "<form>"
      "<select id='color'><option value='red'>red</option><option "
      "value='blue'>blue</option></select>"
      "</form>"
      "</html>");

  std::string change_value =
      "var color = document.getElementById('color');"
      "color.selectedIndex = 1;";

  base::RunLoop run_loop;
  EXPECT_CALL(autofill_driver(),
              SelectControlSelectionChanged(
                  Property(&FormData::fields,
                           ElementsAre(test::FormFieldDescriptionEq(
                               {.name = u"color", .value = u"blue"}))),
                  _))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  // The click simulation is necessary to give the frame transient user
  // activation, otherwise the select value-change event will be ignored by the
  // agent.
  ASSERT_TRUE(SimulateElementClick("color"));
  ExecuteJavaScriptForTests(change_value.c_str());
  std::move(run_loop).Run();
}

}  // namespace

}  // namespace autofill
