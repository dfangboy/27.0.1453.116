// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include "ui/views/test/views_test_base.h"

namespace views {

typedef ViewsTestBase BubbleBorderTest;

TEST_F(BubbleBorderTest, GetMirroredArrow) {
  // Horizontal mirroring.
  EXPECT_EQ(BubbleBorder::TOP_RIGHT,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_LEFT));
  EXPECT_EQ(BubbleBorder::TOP_LEFT,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_RIGHT));

  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_LEFT));
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_EQ(BubbleBorder::RIGHT_TOP,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_TOP));
  EXPECT_EQ(BubbleBorder::LEFT_TOP,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_TOP));

  EXPECT_EQ(BubbleBorder::RIGHT_BOTTOM,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_BOTTOM));
  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_EQ(BubbleBorder::TOP_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::TOP_CENTER));
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::BOTTOM_CENTER));

  EXPECT_EQ(BubbleBorder::RIGHT_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::LEFT_CENTER));
  EXPECT_EQ(BubbleBorder::LEFT_CENTER,
            BubbleBorder::horizontal_mirror(BubbleBorder::RIGHT_CENTER));

  EXPECT_EQ(BubbleBorder::NONE,
            BubbleBorder::horizontal_mirror(BubbleBorder::NONE));
  EXPECT_EQ(BubbleBorder::FLOAT,
            BubbleBorder::horizontal_mirror(BubbleBorder::FLOAT));

  // Vertical mirroring.
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_LEFT));
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_RIGHT));

  EXPECT_EQ(BubbleBorder::TOP_LEFT,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_LEFT));
  EXPECT_EQ(BubbleBorder::TOP_RIGHT,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_TOP));
  EXPECT_EQ(BubbleBorder::RIGHT_BOTTOM,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_TOP));

  EXPECT_EQ(BubbleBorder::LEFT_TOP,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_BOTTOM));
  EXPECT_EQ(BubbleBorder::RIGHT_TOP,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::TOP_CENTER));
  EXPECT_EQ(BubbleBorder::TOP_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::BOTTOM_CENTER));

  EXPECT_EQ(BubbleBorder::LEFT_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::LEFT_CENTER));
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER,
            BubbleBorder::vertical_mirror(BubbleBorder::RIGHT_CENTER));

  EXPECT_EQ(BubbleBorder::NONE,
            BubbleBorder::vertical_mirror(BubbleBorder::NONE));
  EXPECT_EQ(BubbleBorder::FLOAT,
            BubbleBorder::vertical_mirror(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, HasArrow) {
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_LEFT));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_TOP));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_TOP));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_BOTTOM));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::LEFT_CENTER));
  EXPECT_TRUE(BubbleBorder::has_arrow(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::has_arrow(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::has_arrow(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnLeft) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_TOP));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::TOP_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnTop) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_TOP));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(BubbleBorder::TOP_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::BOTTOM_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowOnHorizontal) {
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_RIGHT));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_LEFT));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::BOTTOM_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::LEFT_CENTER));
  EXPECT_FALSE(
      BubbleBorder::is_arrow_on_horizontal(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_horizontal(BubbleBorder::FLOAT));
}

TEST_F(BubbleBorderTest, IsArrowAtCenter) {
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_LEFT));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_RIGHT));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_TOP));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_TOP));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_BOTTOM));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_BOTTOM));

  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::TOP_CENTER));
  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::BOTTOM_CENTER));

  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::LEFT_CENTER));
  EXPECT_TRUE(BubbleBorder::is_arrow_at_center(BubbleBorder::RIGHT_CENTER));

  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::NONE));
  EXPECT_FALSE(BubbleBorder::is_arrow_at_center(BubbleBorder::FLOAT));
}

}  // namespace views
