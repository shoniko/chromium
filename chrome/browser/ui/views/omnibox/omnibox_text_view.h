// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class RenderText;
}  // namespace gfx

// A view containing a render text styled via search results. This differs from
// the general purpose views::Label class by having less general features (such
// as selection) and more specific features (such as suggestion answer styling).
class OmniboxTextView : public views::View {
 public:
  explicit OmniboxTextView(OmniboxResultView* result_view);
  ~OmniboxTextView() override;

  // views::View.
  gfx::Size CalculatePreferredSize() const override;
  bool CanProcessEventsWithinSubtree() const override;
  const char* GetClassName() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Dims the text (i.e. makes it gray). This is used for secondary text (so
  // that the non-dimmed text stands out more).
  void Dim();

  // Returns the render text, or an empty string if there is none.
  const base::string16& text() const;

  // Sets the render text with default rendering for the given |text|. The
  // |classifications| are used to style the text. An ImageLine incorporates
  // both the text and the styling.
  void SetText(const base::string16& text);
  void SetText(const base::string16& text,
               const ACMatchClassifications& classifications);
  void SetText(const SuggestionAnswer::ImageLine& line);

  // Adds the "additional" and "status" text from |line|, if any.
  void AppendExtraText(const SuggestionAnswer::ImageLine& line);

  // Get the height of one line of text.  This is handy if the view might have
  // multiple lines.
  int GetLineHeight() const;

 private:
  std::unique_ptr<gfx::RenderText> CreateRenderText(
      const base::string16& text) const;

  // Adds |text| to the render text.  |text_type| is an index into the
  // kTextStyles constant defined in the .cc file and is used to style the text,
  // including setting the font size, color, and baseline style.  See the
  // TextStyle struct in the .cc file for more.
  void AppendText(const base::string16& text, int text_type);

  // Updates the cached maximum line height.
  void UpdateLineHeight();

  // To get color values.
  OmniboxResultView* result_view_;

  // Font settings for this view.
  int font_height_;

  // Whether to wrap lines if the width is too narrow for the whole string.
  bool wrap_text_lines_;

  // The primary data for this class.
  std::unique_ptr<gfx::RenderText> render_text_;
  // The classifications most recently passed to SetText. Used to exit
  // early instead of setting text when the text and classifications
  // match the current state of the view.
  std::unique_ptr<ACMatchClassifications> cached_classifications_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxTextView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_
