// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/printing_handler.h"

#include <stdint.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_utility_printing_messages.h"
#include "content/public/utility/utility_thread.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"

#if defined(OS_WIN)
#include "base/win/iat_patch_function.h"
#include "printing/emf_win.h"
#include "ui/gfx/gdi_util.h"
#endif

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

}  // namespace

PrintingHandler::PrintingHandler() {}

PrintingHandler::~PrintingHandler() {}

bool PrintingHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintingHandler, message)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles,
                        OnRenderPDFPagesToMetafile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles_GetPage,
                        OnRenderPDFPagesToMetafileGetPage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafiles_Stop,
                        OnRenderPDFPagesToMetafileStop)
#endif  // OS_WIN
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if defined(OS_WIN)
void PrintingHandler::OnRenderPDFPagesToMetafile(
    IPC::PlatformFileForTransit pdf_transit,
    const printing::PdfRenderSettings& settings) {
  pdf_rendering_settings_ = settings;
  base::File pdf_file = IPC::PlatformFileForTransitToFile(pdf_transit);
  int page_count = LoadPDF(std::move(pdf_file));
  Send(
      new ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_PageCount(page_count));
}

void PrintingHandler::OnRenderPDFPagesToMetafileGetPage(
    int page_number,
    IPC::PlatformFileForTransit output_file) {
  base::File emf_file = IPC::PlatformFileForTransitToFile(output_file);
  double scale_factor = 1.0;
  bool success =
      RenderPdfPageToMetafile(page_number, std::move(emf_file), &scale_factor);
  Send(new ChromeUtilityHostMsg_RenderPDFPagesToMetafiles_PageDone(
      success, scale_factor));
}

void PrintingHandler::OnRenderPDFPagesToMetafileStop() {
  ReleaseProcessIfNeeded();
}

int PrintingHandler::LoadPDF(base::File pdf_file) {
  int64_t length = pdf_file.GetLength();
  if (length < 0)
    return 0;

  pdf_data_.resize(length);
  if (length != pdf_file.Read(0, pdf_data_.data(), pdf_data_.size()))
    return 0;

  int total_page_count = 0;
  if (!chrome_pdf::GetPDFDocInfo(
          &pdf_data_.front(), pdf_data_.size(), &total_page_count, NULL)) {
    return 0;
  }
  return total_page_count;
}

bool PrintingHandler::RenderPdfPageToMetafile(int page_number,
                                              base::File output_file,
                                              double* scale_factor) {
  printing::Emf metafile;
  metafile.Init();

  // We need to scale down DC to fit an entire page into DC available area.
  // Current metafile is based on screen DC and have current screen size.
  // Writing outside of those boundaries will result in the cut-off output.
  // On metafiles (this is the case here), scaling down will still record
  // original coordinates and we'll be able to print in full resolution.
  // Before playback we'll need to counter the scaling up that will happen
  // in the service (print_system_win.cc).
  *scale_factor =
      gfx::CalculatePageScale(metafile.context(),
                              pdf_rendering_settings_.area().right(),
                              pdf_rendering_settings_.area().bottom());
  gfx::ScaleDC(metafile.context(), *scale_factor);

  // The underlying metafile is of type Emf and ignores the arguments passed
  // to StartPage.
  metafile.StartPage(gfx::Size(), gfx::Rect(), 1);
  if (!chrome_pdf::RenderPDFPageToDC(
          &pdf_data_.front(),
          pdf_data_.size(),
          page_number,
          metafile.context(),
          pdf_rendering_settings_.dpi(),
          pdf_rendering_settings_.area().x(),
          pdf_rendering_settings_.area().y(),
          pdf_rendering_settings_.area().width(),
          pdf_rendering_settings_.area().height(),
          true,
          false,
          true,
          true,
          pdf_rendering_settings_.autorotate())) {
    return false;
  }
  metafile.FinishPage();
  metafile.FinishDocument();
  return metafile.SaveTo(&output_file);
}

#endif  // OS_WIN

