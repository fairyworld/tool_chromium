// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InkTextAnnotationsElement, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {Ink2Manager} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getTestAnnotation, MockDocumentDimensions, setUpInkTestContext} from './test_util.js';
import type {MockPdfPluginElement} from './test_util.js';

let viewport: Viewport;
let mockPlugin: MockPdfPluginElement;

function setUpInk2Manager(): Ink2Manager {
  Ink2Manager.setInstance(null);
  const context = setUpInkTestContext();
  viewport = context.viewport;
  mockPlugin = context.mockPlugin;
  return Ink2Manager.getInstance();
}

function createAnnotationsElement(): InkTextAnnotationsElement {
  document.body.innerHTML = '';
  const annotationsElement = document.createElement('ink-text-annotations');
  annotationsElement.viewport = viewport;
  document.body.appendChild(annotationsElement);
  return annotationsElement;
}

function getPlaceholders(annotationsElement: InkTextAnnotationsElement):
    NodeListOf<HTMLElement> {
  return annotationsElement.shadowRoot.querySelectorAll('.placeholder');
}

chrome.test.runTests([
  async function testPlaceholdersRenderedAndSorted() {
    const manager = setUpInk2Manager();

    // Add a second page to the document dimensions to support pageIndex 1.
    const dimensions = new MockDocumentDimensions(0, 0);
    dimensions.addPage(400, 500);  // Page 0: 400x500
    dimensions.addPage(400, 500);  // Page 1: 400x500
    viewport.setDocumentDimensions(dimensions);

    // Commit annotations with SCREEN coordinates. Ink2Manager will convert them
    // to PAGE coordinates.
    // Page 0 screen offset: X=55, Y=3. Page 1 screen offset: X=55, Y=503
    // (at 1.0 zoom).

    // Annotation 1: Page 0, Screen X=105, Y=103 (Page X=50, Y=100)
    const annotation1 = {
      ...getTestAnnotation(1),
      text: 'Annotation 1',
      textBoxRect: {height: 20, locationX: 105, locationY: 103, width: 100},
    };

    // Annotation 2: Page 0, Screen X=105, Y=53 (Page X=50, Y=50)
    const annotation2 = {
      ...getTestAnnotation(2),
      text: 'Annotation 2',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };

    // Annotation 3: Page 1, Screen X=65, Y=513 (Page X=10, Y=10)
    const annotation3 = {
      ...getTestAnnotation(3),
      pageIndex: 1,
      text: 'Annotation 3',
      textBoxRect: {height: 20, locationX: 65, locationY: 513, width: 100},
    };

    // Commit them to manager (out of order)
    manager.commitTextAnnotation(annotation1, true, []);
    manager.commitTextAnnotation(annotation3, true, []);
    manager.commitTextAnnotation(annotation2, true, []);

    const annotationsElement = createAnnotationsElement();
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(3, placeholders.length);

    // Verify A11y attributes on inner container
    const container = annotationsElement.shadowRoot.querySelector('#container');
    assert(container);
    chrome.test.assertEq('list', container.getAttribute('role'));
    chrome.test.assertEq(
        'Text Annotations', container.getAttribute('aria-label'));

    // Verify A11y attributes on placeholders
    chrome.test.assertEq('listitem', placeholders[0]!.getAttribute('role'));
    chrome.test.assertEq('0', placeholders[0]!.getAttribute('tabindex'));

    // Verify sorting: annotation2 (Page 0, Y=50) -> annotation1 (Page 0, Y=100)
    // -> annotation3 (Page 1)
    chrome.test.assertEq(
        'Annotation 2', placeholders[0]!.getAttribute('aria-label'));
    chrome.test.assertEq(
        'Annotation 1', placeholders[1]!.getAttribute('aria-label'));
    chrome.test.assertEq(
        'Annotation 3', placeholders[2]!.getAttribute('aria-label'));

    // Assert computed style which includes the CSS offsets (matching
    // ink-text-box): Left: 105 - 12 = 93px Top: 53 - 10 = 43px Width: 100 + 24
    // = 124px Height: 20 + 20 = 40px
    const style2 = window.getComputedStyle(placeholders[0]!);
    chrome.test.assertEq('93px', style2.left);
    chrome.test.assertEq('43px', style2.top);
    chrome.test.assertEq('124px', style2.width);
    chrome.test.assertEq('40px', style2.height);

    chrome.test.succeed();
  },

  async function testPlaceholderFocusedScroll() {
    const manager = setUpInk2Manager();

    const dimensions = new MockDocumentDimensions(0, 0);
    dimensions.addPage(400, 500);  // Page 0: 400x500
    viewport.setDocumentDimensions(dimensions);

    // Commit 3 annotations:
    // 1. Out of viewport in y direction after zoom: page (50, 400), screen
    // (105, 403).
    // 2. Out of viewport in x direction after zoom: page (250, 100), screen
    // (305, 103).
    // 3. Still in viewport after zoom: page (50, 50), screen (105, 53).
    //
    // Sorted order (top-to-bottom, left-to-right):
    // - Annotation 3 (Page Y=50) -> Placeholder 0
    // - Annotation 2 (Page Y=100) -> Placeholder 1
    // - Annotation 1 (Page Y=400) -> Placeholder 2

    const annotation1 = {
      ...getTestAnnotation(1),
      text: 'Scroll Y',
      textBoxRect: {height: 20, locationX: 105, locationY: 403, width: 100},
    };
    const annotation2 = {
      ...getTestAnnotation(2),
      text: 'Scroll X',
      textBoxRect: {height: 20, locationX: 305, locationY: 103, width: 100},
    };
    const annotation3 = {
      ...getTestAnnotation(3),
      text: 'No Scroll',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };

    manager.commitTextAnnotation(annotation1, true, []);
    manager.commitTextAnnotation(annotation2, true, []);
    manager.commitTextAnnotation(annotation3, true, []);

    const annotationsElement = createAnnotationsElement();
    await microtasksFinished();

    // Zoom to 2.0.
    viewport.setZoom(2.0);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(3, placeholders.length);

    // --- Case 1: Focus Annotation 3 (No Scroll) ---
    // Textbox is at Page X = 50, Page Y = 50.
    // At 2.0x: screenRect is X: [110, 310], Y: [106, 146].
    // This is within viewport 500x500, so no scroll should occur.
    mockPlugin.clearMessages();
    placeholders[0]!.focus();
    placeholders[0]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();

    let syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertEq(undefined, syncScrollMessage);

    // --- Case 2: Focus Annotation 2 (Scroll X) ---
    // Textbox Page X = 250, Page Y = 100.
    // At 2.0x zoom: screenRect.locationX = 510, width = 200.
    // screenRect.locationY = 206, height = 40.
    // Scrolls X to 305 (clamped from 460). Y remains 0.
    mockPlugin.clearMessages();
    placeholders[1]!.focus();
    placeholders[1]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();

    syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    chrome.test.assertEq(305, syncScrollMessage.x);
    chrome.test.assertEq(0, syncScrollMessage.y);

    // Reset viewport position to (0,0) for next test.
    viewport.setPosition({x: 0, y: 0});
    await microtasksFinished();

    // --- Case 3: Focus Annotation 1 (Scroll Y) ---
    // Textbox Page X = 50, Page Y = 400.
    // At 2.0x zoom: screenRect.locationX = 110, width = 200.
    // screenRect.locationY = 806, height = 40.
    // Scrolls Y to 505 (clamped from 756). X remains 0.
    mockPlugin.clearMessages();
    placeholders[2]!.focus();
    placeholders[2]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();

    syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    chrome.test.assertEq(0, syncScrollMessage.x);
    chrome.test.assertEq(505, syncScrollMessage.y);

    chrome.test.succeed();
  },
]);
