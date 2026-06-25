// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import 'chrome://resources/cr_components/searchbox/searchbox_input.js';

import {createAutocompleteResultForTesting, createSearchMatchForTesting, SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from 'chrome://resources/cr_components/searchbox/searchbox_input.js';
import {SearchboxMixin} from 'chrome://resources/cr_components/searchbox/searchbox_mixin.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createUrlMatch} from './searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

const TestElementBase = SearchboxMixin(CrLitElement);

interface TestSearchboxMixinElement {
  $: {
    input: SearchboxInputElement,
    matches: SearchboxDropdownElement,
    inputWrapper: HTMLElement,
  };
}

class TestSearchboxMixinElement extends TestElementBase {
  static get is() {
    return 'test-searchbox-mixin';
  }

  override render() {
    return html`
      <div id="inputWrapper"
          @focusout="${this.onInputWrapperFocusout}"
          @keydown="${this.onInputWrapperKeydown}">
        <cr-searchbox-input id="input"
            .result="${this.result}"
            @input-focus-changed="${this.onInputFocusChanged}"
            @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated}">
        </cr-searchbox-input>
        <cr-searchbox-dropdown id="matches"
            .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @match-focusin="${this.onMatchFocusin}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}">
        </cr-searchbox-dropdown>
      </div>
    `;
  }

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    return this.$.matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler() {
    return SearchboxBrowserProxy.getInstance().handler;
  }
}

customElements.define(TestSearchboxMixinElement.is, TestSearchboxMixinElement);

function simulateUserTextInput(
    inputElement: SearchboxInputElement, value: string): Promise<void> {
  inputElement.inputElement.value = value;
  inputElement.fire('searchbox-input-text-updated', {
    value,
    isComposing: false,
  });
  return microtasksFinished();
}

suite('SearchboxMixinTest', () => {
  let element: TestSearchboxMixinElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    element = document.createElement('test-searchbox-mixin') as
        TestSearchboxMixinElement;
    document.body.appendChild(element);
  });

  test('autocomplete should not query for empty inputs', async () => {
    const inputElement = element.getInputElement();
    await simulateUserTextInput(inputElement, 'he');

    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    // Deleting a character still queries autocomplete.
    await simulateUserTextInput(inputElement, 'h');

    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));

    // Deleting a character does not query autocomplete for empty input.
    await simulateUserTextInput(inputElement, '');
    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));

    // Typing space does not query autocomplete.
    await simulateUserTextInput(inputElement, ' ');
    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test(
      'onInputFocusChanged queries autocomplete if dropdown is not visible',
      async () => {
        element.dropdownIsVisible = false;
        element.getInputElement().fire('input-focus-changed', {value: 'hello'});
        await microtasksFinished();
        const args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello');

        testProxy.handler.reset();

        element.dropdownIsVisible = true;
        element.getInputElement().fire('input-focus-changed', {value: 'hello'});
        await microtasksFinished();
        assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test(
      'arrow up/down keys query autocomplete when dropdown is not visible',
      async () => {
        element.dropdownIsVisible = false;
        const mockInput = element.getInputElement();
        mockInput.inputElement.value = '';

        element.getWrapperElement().dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowDown',
          bubbles: true,
          cancelable: true,
        }));
        await microtasksFinished();

        let args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, '');
        assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

        testProxy.handler.reset();

        mockInput.inputElement.value = 'hello';
        element.getWrapperElement().dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowUp',
          bubbles: true,
          cancelable: true,
        }));
        await microtasksFinished();

        args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello');
        assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

        testProxy.handler.reset();

        // Does not query autocomplete when dropdown is visible.
        element.dropdownIsVisible = true;
        element.getWrapperElement().dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowDown',
          bubbles: true,
          cancelable: true,
        }));
        await microtasksFinished();
        assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test('mousedown on empty input queries zero-prefix suggestions', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 0, bubbles: true, composed: true}));
    await microtasksFinished();

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, '');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    element.dropdownIsVisible = true;
    element.result = createAutocompleteResultForTesting({
      matches: [createSearchMatchForTesting(), createUrlMatch()],
    });
    await microtasksFinished();

    // Arrow up/down keys do not query autocomplete when matches are showing.
    element.getWrapperElement().dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'ArrowUp',
    }));
    await microtasksFinished();
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('queryAutocomplete passes cursor position', async () => {
    const inputElement = element.getInputElement();
    inputElement.inputElement.value = 'hello';
    inputElement.inputElement.selectionStart = 3;
    inputElement.inputElement.selectionEnd = 3;

    element.queryAutocomplete('hello', /*preventInlineAutocomplete=*/ false);

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'hello');
    assertEquals(args.cursorPosition, 3);
  });

  test(
      'queryAutocomplete passes cursor position when input is out of sync',
      async () => {
        const inputElement = element.getInputElement();
        inputElement.inputElement.value = 'hello';
        inputElement.inputElement.selectionStart = 3;
        inputElement.inputElement.selectionEnd = 3;

        element.queryAutocomplete(
            'hello world', /*preventInlineAutocomplete=*/ false);

        const args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello world');
        assertEquals(args.cursorPosition, 11);
      });

  test('clearing the input stops autocomplete', async () => {
    const inputElement = element.getInputElement();
    await simulateUserTextInput(inputElement, 'h');

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'h');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    await simulateUserTextInput(inputElement, '');

    args = await testProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult);
  });

  test('stale autocomplete response is ignored', async () => {
    element.queryAutocomplete('he', /*preventInlineAutocomplete=*/ false);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      input: 'h',  // Simulate stale response.
      matches: matches,
    }));
    await microtasksFinished();

    assertFalse(element.dropdownIsVisible);
    assertEquals(null, element.result);
  });

  test('arrow events are sent to handler', async () => {
    const inputElement = element.getInputElement();
    await simulateUserTextInput(inputElement, 'he');

    const matches = [createSearchMatchForTesting()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      input: 'he',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const arrowDownEvent = new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      bubbles: true,
      cancelable: true,
    });
    element.getWrapperElement().dispatchEvent(arrowDownEvent);
    await microtasksFinished();

    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(0, args.line);
    assertEquals(
        NavigationPredictor.kUpOrDownArrowButton, args.navigationPredictor);
  });

  test('keyboard modifier keys behavior', async () => {
    const metaZEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'z',
      metaKey: true,
    });
    const ctrlZEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'z',
      ctrlKey: true,
    });

    let metaZStopped = false;
    let ctrlZStopped = false;
    metaZEvent.stopPropagation = () => {
      metaZStopped = true;
    };
    ctrlZEvent.stopPropagation = () => {
      ctrlZStopped = true;
    };

    element.getWrapperElement().dispatchEvent(metaZEvent);
    element.getWrapperElement().dispatchEvent(ctrlZEvent);
    await microtasksFinished();

    assertEquals(isMac, metaZStopped);
    assertEquals(!isMac, ctrlZStopped);
    assertFalse(metaZEvent.defaultPrevented);
    assertFalse(ctrlZEvent.defaultPrevented);
  });

  test('pressing Enter in empty input prevents new line', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    element.queryAutocomplete('', false);
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      input: '',
      matches: [createSearchMatchForTesting()],
    }));
    await microtasksFinished();

    const enterEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'Enter',
    });

    element.getWrapperElement().dispatchEvent(enterEvent);
    await microtasksFinished();

    assertTrue(enterEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('onMatchFocusin selects match and updates input', async () => {
    const matches = [createSearchMatchForTesting({
      fillIntoEdit: 'test fill',
    })];
    element.result = createAutocompleteResultForTesting({
      matches: matches,
    });
    element.selectedMatchIndex = 0;
    element.selectedMatch = matches[0] || null;
    await microtasksFinished();

    element.getDropdownElement().fire('match-focusin', 0);
    await microtasksFinished();

    assertEquals(0, element.selectedMatchIndex);
    assertEquals('test fill', element.getInputElement().inputElement.value);
  });

  test(
      'onInputWrapperFocusout stops autocomplete or clears matches',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.inputElement.value = 'hello';
        element.lastQueriedInput = 'hello';
        element.dropdownIsVisible = true;
        await microtasksFinished();

        // Focus stays inside wrapper.
        element.getWrapperElement().dispatchEvent(new FocusEvent('focusout', {
          relatedTarget: element.getInputElement().inputElement,
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);
        assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));

        // Focus goes outside wrapper for non-empty input.
        element.getWrapperElement().dispatchEvent(new FocusEvent('focusout', {
          relatedTarget: document.body,
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);
        assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));

        testProxy.handler.reset();

        // Focus goes outside wrapper for empty input.
        element.lastQueriedInput = '';
        element.dropdownIsVisible = true;
        element.getWrapperElement().dispatchEvent(new FocusEvent('focusout', {
          relatedTarget: document.body,
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);
        assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
      });
});
