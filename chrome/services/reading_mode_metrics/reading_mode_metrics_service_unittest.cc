// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/reading_mode_metrics/reading_mode_metrics_service.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/services/reading_mode_metrics/reading_mode_metrics.rs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

namespace reading_mode {

class ReadingModeMetricsServiceTest : public testing::Test {
 protected:
  ui::AXTreeUpdate CreateMockAXTreeUpdate() {
    ui::AXTreeUpdate update;

    // 1. Root Web Area
    ui::AXNodeData root;
    root.id = 1;
    root.role = ax::mojom::Role::kRootWebArea;

    // 2. Heading (H1)
    ui::AXNodeData heading;
    heading.id = 2;
    heading.role = ax::mojom::Role::kHeading;

    ui::AXNodeData heading_text;
    heading_text.id = 8;
    heading_text.role = ax::mojom::Role::kStaticText;
    heading_text.SetName("My Article Title");

    // 3. List block
    ui::AXNodeData list;
    list.id = 3;
    list.role = ax::mojom::Role::kList;

    // 4. List item
    ui::AXNodeData list_item;
    list_item.id = 4;
    list_item.role = ax::mojom::Role::kListItem;

    ui::AXNodeData list_item_text;
    list_item_text.id = 9;
    list_item_text.role = ax::mojom::Role::kStaticText;
    list_item_text.SetName("Shopping item");

    // 5. Paragraph block containing formatted static text leaves
    ui::AXNodeData paragraph;
    paragraph.id = 5;
    paragraph.role = ax::mojom::Role::kParagraph;

    // 6. Bold leaf node
    ui::AXNodeData bold_leaf;
    bold_leaf.id = 6;
    bold_leaf.role = ax::mojom::Role::kStaticText;
    bold_leaf.SetName("This is bold content");
    bold_leaf.AddTextStyle(ax::mojom::TextStyle::kBold);

    // 7. Italic leaf node
    ui::AXNodeData italic_leaf;
    italic_leaf.id = 7;
    italic_leaf.role = ax::mojom::Role::kStaticText;
    italic_leaf.SetName("This is italic content");
    italic_leaf.AddTextStyle(ax::mojom::TextStyle::kItalic);

    // Setup parent-child links
    root.child_ids = {2, 3, 5};
    heading.child_ids = {8};
    list.child_ids = {4};
    list_item.child_ids = {9};
    paragraph.child_ids = {6, 7};

    update.nodes = {root,      heading,   heading_text,
                    list,      list_item, list_item_text,
                    paragraph, bold_leaf, italic_leaf};
    update.root_id = 1;
    return update;
  }
};

TEST_F(ReadingModeMetricsServiceTest, ExtractsAXTreeStructuralProperties) {
  ui::AXTree tree(CreateMockAXTreeUpdate());
  OriginalStructure structure;

  // Run structural extraction using the exposed testing wrapper.
  ExtractOriginalStructureForTesting(tree.root(), structure,
                                     /*inside_code=*/false);

  // 1. Validate Headings
  ASSERT_EQ(structure.headings.size(), 1u);
  EXPECT_EQ(std::string(structure.headings[0]), "My Article Title");

  // 2. Validate Grouped Lists
  ASSERT_EQ(structure.lists.size(), 1u);
  ASSERT_EQ(structure.lists[0].items.size(), 1u);
  EXPECT_EQ(std::string(structure.lists[0].items[0]), "Shopping item");

  // 3. Validate Styles Formatting
  ASSERT_EQ(structure.bold_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.bold_fragments[0]), "This is bold content");

  ASSERT_EQ(structure.italic_fragments.size(), 1u);
  EXPECT_EQ(std::string(structure.italic_fragments[0]),
            "This is italic content");
}

}  // namespace reading_mode
