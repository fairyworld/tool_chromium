// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {CreateSkillRequest, GlicBrowserHost, ObservableValue, Skill, SkillPreview, SkillsWebClientEvent, UpdateSkillRequest} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import type {WebClientInitialStatePrivate} from '../request_types.js';
import type {MessageHandlerInterface} from '../transport/messaging.js';
import type {PendingReceiver, PendingRemote, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import {SkillsClientDef} from './skills_types.js';
import type {SkillsClient, SkillsHost} from './skills_types.js';

type Constructor<T = {}> = new (...args: any[]) => T;

interface SkillsHostInterface {
  skillPreviews: ObservableValueImpl<SkillPreview[]>;
  skillToInvoke: ObservableValueImpl<Skill>;
}

export class SkillsWebClientMessageHandler implements
    MessageHandlerInterface<SkillsClient> {
  private cachedSkillPreviews: SkillPreview[] = [];
  private cachedContextualSkillPreviews: SkillPreview[] = [];
  private cachedSkillPrompts = new Map<string, string>();

  constructor(private host: SkillsHostInterface) {}

  notifySkillPreviewsChanged(payload: {
    skillPreviews: SkillPreview[],
  }): void {
    this.cachedSkillPrompts.clear();
    this.cachedSkillPreviews = payload.skillPreviews;
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifyContextualSkillPreviewsChanged(payload: {
    contextualSkillPreviews: SkillPreview[],
  }): void {
    this.cachedContextualSkillPreviews = payload.contextualSkillPreviews;
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillPreviewChanged(payload: {
    skillPreview: SkillPreview,
  }): void {
    const skillPreview = payload.skillPreview;
    this.cachedSkillPrompts.delete(skillPreview.id);

    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillPreview.id);

    if (index !== -1) {
      // SkillPreview with the same ID exists, replace it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        skillPreview,
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    } else {
      // SkillPreview with this ID not found, add it to the cache.
      this.cachedSkillPreviews = [...this.cachedSkillPreviews, skillPreview];
    }

    // Signal the change to the host.
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillDeleted(payload: {
    skillId: string,
  }): void {
    const skillId = payload.skillId;
    this.cachedSkillPrompts.delete(skillId);
    const index = this.cachedSkillPreviews.findIndex(
        (cachedSkillPreview) => cachedSkillPreview.id === skillId);
    if (index !== -1) {
      // SkillPreview with the same ID exists, remove it.
      this.cachedSkillPreviews = [
        ...this.cachedSkillPreviews.slice(0, index),
        ...this.cachedSkillPreviews.slice(index + 1),
      ];
    }
    this.host.skillPreviews.assignAndSignal(this.combineSkillPreviews());
  }

  notifySkillToInvokeChanged(payload: {
    skill: Skill,
  }): void {
    this.host.skillToInvoke.assignAndSignal(payload.skill);
  }

  cacheSkillPrompt(skill: Skill) {
    const preview = skill.preview;
    if (preview.id && skill.prompt) {
      this.cachedSkillPrompts.set(preview.id, skill.prompt);
    }
  }

  private combineSkillPreviews() {
    return [...this.cachedContextualSkillPreviews, ...this.cachedSkillPreviews];
  }
}

export function glicBrowserHostSkillsMixin<T extends Constructor>(base: T) {
  return class extends base implements Partial<GlicBrowserHost> {
    skillsRemote?: PostMessageRemote<SkillsHost>;
    skillsWebClientMessageHandler = new SkillsWebClientMessageHandler(
        this as unknown as SkillsHostInterface);
    skillPreviews = ObservableValueImpl.withNoValue<SkillPreview[]>();
    skillToInvoke = ObservableValueImpl.withNoValue<Skill>();

    initializeSkills(
        initialState: WebClientInitialStatePrivate, router: PostMessageRouter,
        skillsRemote: PendingRemote<SkillsHost>|undefined,
        skillsReceiver: PendingReceiver<SkillsClient>|undefined) {
      if (!initialState.enableSkills) {
        this.createSkill = undefined;
        this.updateSkill = undefined;
        this.showManageSkillsUi = undefined;
        this.showBrowseSkillsUi = undefined;
        this.getSkill = undefined;
        return;
      }

      if (skillsRemote && skillsReceiver) {
        this.skillsRemote = router.newRemote(skillsRemote);
        router.newReceiver(
            skillsReceiver, this.skillsWebClientMessageHandler,
            SkillsClientDef);
      }
    }

    async createSkill?(request: CreateSkillRequest): Promise<void> {
      assert(this.skillsRemote);
      const result =
          await this.skillsRemote.requestWithResponse('createSkill', {request});
      if (!result.modalOpened) {
        throw new Error('createSkill: failed to open dialog');
      }
    }

    async updateSkill?(request: UpdateSkillRequest): Promise<void> {
      assert(this.skillsRemote);
      const result =
          await this.skillsRemote.requestWithResponse('updateSkill', {request});
      if (!result.modalOpened) {
        throw new Error('updateSkill: failed to open dialog');
      }
    }

    showManageSkillsUi?(): void {
      assert(this.skillsRemote);
      this.skillsRemote.requestNoResponse('showManageSkillsUi', undefined);
    }

    showBrowseSkillsUi?(): void {
      assert(this.skillsRemote);
      this.skillsRemote.requestNoResponse('showBrowseSkillsUi', undefined);
    }

    async getSkill?(id: string): Promise<Skill> {
      assert(this.skillsRemote);
      const result =
          await this.skillsRemote.requestWithResponse('getSkill', {id});
      if (!result.skill) {
        throw new Error('getSkill: failed');
      }
      this.skillsWebClientMessageHandler.cacheSkillPrompt(result.skill);
      return result.skill;
    }

    recordSkillsWebClientEvent?(event: SkillsWebClientEvent): void {
      assert(this.skillsRemote);
      this.skillsRemote.requestNoResponse(
          'recordSkillsWebClientEvent', {event});
    }

    getSkillPreviews?(): ObservableValue<SkillPreview[]> {
      return this.skillPreviews;
    }

    getSkillToInvoke?(): ObservableValue<Skill> {
      return this.skillToInvoke;
    }
  };
}
