// fifastreet - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/ui/window.h>
#include <rex/input/input_system.h>
#include <rex/ui/imgui_dialog.h>

#include <cstdlib>
#include <filesystem>
#include <memory>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_IPHONE
#define FIFASTREET_IOS 1
#else
#define FIFASTREET_IOS 0
#endif
#if defined(FIFASTREET_TOUCH)
#include "ios/touch_pad.h"
#endif

class FifastreetApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<FifastreetApp>(new FifastreetApp(ctx, "fifastreet",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
#if defined(FIFASTREET_TOUCH)
    // On-screen football gamepad next to the SDK's pad and keyboard drivers (user 0).
    rexios::touch::Labels labels;
    labels.a = "Pass";
    labels.b = "Lob";
    labels.x = "Shoot";
    labels.y = "Through";
    labels.lt = "Control";  // street ball control
    labels.rt = "Sprint";
    labels.lb = "LB";
    labels.rb = "Flick";
    labels.right_stick = "Tricks";
    rexios::touch::SetLabels(labels);
    config.input_factory = [](bool tool_mode) -> std::unique_ptr<rex::system::IInputSystem> {
      auto input = rex::input::CreateDefaultInputSystem(tool_mode);
      if (!tool_mode) input->AddDriver(rexios::touch::CreateDriver());
      return input;
    };
#endif
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
#if FIFASTREET_IOS
    // iPhone / iPad: the app bundle is read-only. The extracted ISO, saves and caches live in
    // the app's Documents folder (visible in Finder / the Files app).
    if (const char* home = std::getenv("HOME")) {
      const std::filesystem::path docs = std::filesystem::path(home) / "Documents";
      if (paths.game_data_root.empty()) paths.game_data_root = docs / "GameData";
      if (paths.user_data_root.empty()) paths.user_data_root = docs / "userdata";
      if (paths.cache_root.empty()) paths.cache_root = docs / "cache";
    }
#endif
    if (paths.game_data_root.empty()) {
      const auto executable_folder = rex::filesystem::GetExecutableFolder();
      for (const auto& candidate : {executable_folder, executable_folder / "GameData",
                                   executable_folder.parent_path() / "GameData",
                                   executable_folder.parent_path()}) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate / "default.xex", error)) {
          paths.game_data_root = candidate;
          break;
        }
      }
    }
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    window()->SetTitle("FIFA Street PC");
#if defined(FIFASTREET_TOUCH)
    touch_overlay_ = rexios::touch::CreateOverlay(drawer);
#else
    (void)drawer;
#endif
  }

  void OnShutdown() override { touch_overlay_.reset(); }

  void OnPostSetup() override {
    // FIFA Street uses texture fetch descriptors with the invalid-texture tag.
    // Their address and format fields are still valid and must be decoded.
    rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnPreSetup(rex::RuntimeConfig& config) override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnConfigurePaths(rex::PathConfig& paths) override {}

 private:
  std::unique_ptr<rex::ui::ImGuiDialog> touch_overlay_;
};
