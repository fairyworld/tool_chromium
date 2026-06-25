// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks.css.js';
import {getHtml} from './memory_banks.html.js';

export class MemoryBanksElement extends CrLitElement {
  static get is() {
    return 'memory-banks';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entries: {type: Array},
    };
  }

  accessor entries: MemoryBankEntry[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.fetchEntries();
  }

  private async fetchEntries() {
    const {entries} =
        await BrowserProxyImpl.getInstance().handler.getAllEntries();
    this.entries = entries;
  }

  protected get recentlySaved_(): MemoryBankEntry[] {
    return this.entries.slice(0, 3);
  }

  protected getTruncatedText_(text: string|null|undefined): string {
    if (!text) {
      return '';
    }
    const limit = 140;  // Estimated safe limit for 5 lines
    if (text.length <= limit) {
      return text;
    }
    return text.slice(0, limit) + '...';
  }

  protected convertMojoTimeToDate_(mojoTime: {internalValue: bigint}): Date {
    // Mojo Time represents microseconds since the Windows epoch (January 1,
    // 1601). JavaScript Date expects milliseconds since the Unix epoch (January
    // 1, 1970).

    // 11,644,473,600,000,000n is the Windows-to-Unix epoch delta in
    // microseconds.
    const unixEpochUs = mojoTime.internalValue - 11644473600000000n;
    return new Date(Number(unixEpochUs / 1000n));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks': MemoryBanksElement;
  }
}

customElements.define(MemoryBanksElement.is, MemoryBanksElement);
