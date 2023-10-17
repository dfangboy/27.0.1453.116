// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

class MenuControllerTest : public CocoaTest {
};

// A menu delegate that counts the number of times certain things are called
// to make sure things are hooked up properly.
class Delegate : public ui::SimpleMenuModel::Delegate {
 public:
  Delegate()
      : execute_count_(0),
        enable_count_(0),
        menu_to_close_(nil),
        did_show_(false),
        did_close_(false) {
  }

  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    ++enable_count_;
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE { return false; }
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE {
    ++execute_count_;
  }

  virtual void MenuWillShow(ui::SimpleMenuModel* /*source*/) OVERRIDE {
    EXPECT_FALSE(did_show_);
    EXPECT_FALSE(did_close_);
    did_show_ = true;
    NSArray* modes = [NSArray arrayWithObjects:NSEventTrackingRunLoopMode,
                                               NSDefaultRunLoopMode,
                                               nil];
    [menu_to_close_ performSelector:@selector(cancelTracking)
                         withObject:nil
                         afterDelay:0.1
                            inModes:modes];
  }

  virtual void MenuClosed(ui::SimpleMenuModel* /*source*/) OVERRIDE {
    EXPECT_TRUE(did_show_);
    EXPECT_FALSE(did_close_);
    did_close_ = true;
  }

  int execute_count_;
  mutable int enable_count_;
  // The menu on which to call |-cancelTracking| after a short delay in
  // MenuWillShow.
  NSMenu* menu_to_close_;
  bool did_show_;
  bool did_close_;
};

// Just like Delegate, except the items are treated as "dynamic" so updates to
// the label/icon in the model are reflected in the menu.
class DynamicDelegate : public Delegate {
 public:
  DynamicDelegate() {}
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE {
    return true;
  }
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE {
    return label_;
  }
  virtual bool GetIconForCommandId(
      int command_id,
      gfx::Image* icon) const OVERRIDE {
    if (icon_.IsEmpty()) {
      return false;
    } else {
      *icon = icon_;
      return true;
    }
  }
  void SetDynamicLabel(string16 label) { label_ = label; }
  void SetDynamicIcon(const gfx::Image& icon) { icon_ = icon; }

 private:
  string16 label_;
  gfx::Image icon_;
};

TEST_F(MenuControllerTest, EmptyMenu) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 0);
}

TEST_F(MenuControllerTest, BasicCreation) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  model.AddItem(3, ASCIIToUTF16("three"));
  model.AddSeparator(ui::NORMAL_SEPARATOR);
  model.AddItem(4, ASCIIToUTF16("four"));
  model.AddItem(5, ASCIIToUTF16("five"));

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 6);

  // Check the title, tag, and represented object are correct for a random
  // element.
  NSMenuItem* itemTwo = [[menu menu] itemAtIndex:2];
  NSString* title = [itemTwo title];
  EXPECT_EQ(ASCIIToUTF16("three"), base::SysNSStringToUTF16(title));
  EXPECT_EQ([itemTwo tag], 2);
  EXPECT_EQ([[itemTwo representedObject] pointerValue], &model);

  EXPECT_TRUE([[[menu menu] itemAtIndex:3] isSeparatorItem]);
}

TEST_F(MenuControllerTest, Submenus) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  ui::SimpleMenuModel submodel(&delegate);
  submodel.AddItem(2, ASCIIToUTF16("sub-one"));
  submodel.AddItem(3, ASCIIToUTF16("sub-two"));
  submodel.AddItem(4, ASCIIToUTF16("sub-three"));
  model.AddSubMenuWithStringId(5, IDS_ZOOM_MENU, &submodel);
  model.AddItem(6, ASCIIToUTF16("three"));

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 3);

  // Inspect the submenu to ensure it has correct properties.
  NSMenu* submenu = [[[menu menu] itemAtIndex:1] submenu];
  EXPECT_TRUE(submenu);
  EXPECT_EQ([submenu numberOfItems], 3);

  // Inspect one of the items to make sure it has the correct model as its
  // represented object and the proper tag.
  NSMenuItem* submenuItem = [submenu itemAtIndex:1];
  NSString* title = [submenuItem title];
  EXPECT_EQ(ASCIIToUTF16("sub-two"), base::SysNSStringToUTF16(title));
  EXPECT_EQ([submenuItem tag], 1);
  EXPECT_EQ([[submenuItem representedObject] pointerValue], &submodel);

  // Make sure the item after the submenu is correct and its represented
  // object is back to the top model.
  NSMenuItem* item = [[menu menu] itemAtIndex:2];
  title = [item title];
  EXPECT_EQ(ASCIIToUTF16("three"), base::SysNSStringToUTF16(title));
  EXPECT_EQ([item tag], 2);
  EXPECT_EQ([[item representedObject] pointerValue], &model);
}

TEST_F(MenuControllerTest, EmptySubmenu) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  ui::SimpleMenuModel submodel(&delegate);
  model.AddSubMenuWithStringId(2, IDS_ZOOM_MENU, &submodel);

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 2);
}

TEST_F(MenuControllerTest, PopUpButton) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  model.AddItem(3, ASCIIToUTF16("three"));

  // Menu should have an extra item inserted at position 0 that has an empty
  // title.
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:YES]);
  EXPECT_EQ([[menu menu] numberOfItems], 4);
  EXPECT_EQ(base::SysNSStringToUTF16([[[menu menu] itemAtIndex:0] title]),
            string16());

  // Make sure the tags are still correct (the index no longer matches the tag).
  NSMenuItem* itemTwo = [[menu menu] itemAtIndex:2];
  EXPECT_EQ([itemTwo tag], 1);
}

TEST_F(MenuControllerTest, Execute) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 1);

  // Fake selecting the menu item, we expect the delegate to be told to execute
  // a command.
  NSMenuItem* item = [[menu menu] itemAtIndex:0];
  [[item target] performSelector:[item action] withObject:item];
  EXPECT_EQ(delegate.execute_count_, 1);
}

void Validate(MenuController* controller, NSMenu* menu) {
  for (int i = 0; i < [menu numberOfItems]; ++i) {
    NSMenuItem* item = [menu itemAtIndex:i];
    [controller validateUserInterfaceItem:item];
    if ([item hasSubmenu])
      Validate(controller, [item submenu]);
  }
}

TEST_F(MenuControllerTest, Validate) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  ui::SimpleMenuModel submodel(&delegate);
  submodel.AddItem(2, ASCIIToUTF16("sub-one"));
  model.AddSubMenuWithStringId(3, IDS_ZOOM_MENU, &submodel);

  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 3);

  Validate(menu.get(), [menu menu]);
}

TEST_F(MenuControllerTest, DefaultInitializer) {
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("one"));
  model.AddItem(2, ASCIIToUTF16("two"));
  model.AddItem(3, ASCIIToUTF16("three"));

  scoped_nsobject<MenuController> menu([[MenuController alloc] init]);
  EXPECT_FALSE([menu menu]);

  [menu setModel:&model];
  [menu setUseWithPopUpButtonCell:NO];
  EXPECT_TRUE([menu menu]);
  EXPECT_EQ(3, [[menu menu] numberOfItems]);

  // Check immutability.
  model.AddItem(4, ASCIIToUTF16("four"));
  EXPECT_EQ(3, [[menu menu] numberOfItems]);
}

// Test that menus with dynamic labels actually get updated.
TEST_F(MenuControllerTest, Dynamic) {
  DynamicDelegate delegate;

  // Create a menu containing a single item whose label is "initial" and who has
  // no icon.
  string16 initial = ASCIIToUTF16("initial");
  delegate.SetDynamicLabel(initial);
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("foo"));
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  EXPECT_EQ([[menu menu] numberOfItems], 1);
  // Validate() simulates opening the menu - the item label/icon should be
  // initialized after this so we can validate the menu contents.
  Validate(menu.get(), [menu menu]);
  NSMenuItem* item = [[menu menu] itemAtIndex:0];
  // Item should have the "initial" label and no icon.
  EXPECT_EQ(initial, base::SysNSStringToUTF16([item title]));
  EXPECT_EQ(nil, [item image]);

  // Now update the item to have a label of "second" and an icon.
  string16 second = ASCIIToUTF16("second");
  delegate.SetDynamicLabel(second);
  const gfx::Image& icon =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_THROBBER);
  delegate.SetDynamicIcon(icon);
  // Simulate opening the menu and validate that the item label + icon changes.
  Validate(menu.get(), [menu menu]);
  EXPECT_EQ(second, base::SysNSStringToUTF16([item title]));
  EXPECT_TRUE([item image] != nil);

  // Now get rid of the icon and make sure it goes away.
  delegate.SetDynamicIcon(gfx::Image());
  Validate(menu.get(), [menu menu]);
  EXPECT_EQ(second, base::SysNSStringToUTF16([item title]));
  EXPECT_EQ(nil, [item image]);
}

TEST_F(MenuControllerTest, OpenClose) {
  // ui::SimpleMenuModel posts a task that calls Delegate::MenuClosed. Create
  // a MessageLoop for that purpose.
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  // Create the model.
  Delegate delegate;
  ui::SimpleMenuModel model(&delegate);
  model.AddItem(1, ASCIIToUTF16("allays"));
  model.AddItem(2, ASCIIToUTF16("i"));
  model.AddItem(3, ASCIIToUTF16("bf"));

  // Create the controller.
  scoped_nsobject<MenuController> menu(
      [[MenuController alloc] initWithModel:&model useWithPopUpButtonCell:NO]);
  delegate.menu_to_close_ = [menu menu];

  EXPECT_FALSE([menu isMenuOpen]);

  // In the event tracking run loop mode of the menu, verify that the controller
  // resports the menu as open.
  CFRunLoopPerformBlock(CFRunLoopGetCurrent(), NSEventTrackingRunLoopMode, ^{
      EXPECT_TRUE([menu isMenuOpen]);
  });

  // Pop open the menu, which will spin an event-tracking run loop.
  [NSMenu popUpContextMenu:[menu menu]
                 withEvent:nil
                   forView:[test_window() contentView]];

  EXPECT_FALSE([menu isMenuOpen]);

  // When control returns back to here, the menu will have finished running its
  // loop and will have closed itself (see Delegate::MenuWillShow).
  EXPECT_TRUE(delegate.did_show_);

  // When the menu tells the Model it closed, the Model posts a task to notify
  // the delegate. But since this is a test and there's no running MessageLoop,
  // |did_close_| will remain false until we pump the task manually.
  EXPECT_FALSE(delegate.did_close_);

  // Pump the task that notifies the delegate.
  message_loop.RunUntilIdle();

  // Expect that the delegate got notified properly.
  EXPECT_TRUE(delegate.did_close_);
}

}  // namespace
