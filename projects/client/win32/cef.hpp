#include "cef_app.h"
#include "cef_client.h"
#include "wrapper/cef_helpers.h"

#define CEF_DEFAULT_GFX_LAYER 200
gfx_handle g_texture = -1;

class RenderHandler :
    public CefRenderHandler
{
    public:
        unsigned char* mPixelBuffer;
        int mPixelBufferWidth;
        int mPixelBufferHeight;
        unsigned char* mPopupBuffer;
        unsigned char* mPixelBufferRow;
        CefRect mPopupBufferRect;
        int mBufferDepth;
        bool mFlipYPixels;

        RenderHandler() {
            // inidcates if we should flip the pixel buffer in Y direction
            mFlipYPixels = false;

            // the pixel buffer
            mPixelBuffer = nullptr;
            mPixelBufferWidth = 0;
            mPixelBufferHeight = 0;

            // the popup buffer
            mPopupBuffer = nullptr;

            // a row in the pixel buffer - used as temp buffer when flipping
            mPixelBufferRow = nullptr;

            // depth is same for all buffer
            mBufferDepth = 4;
        }

        ~RenderHandler() {
            delete[] mPixelBuffer;
            delete[] mPopupBuffer;
            delete[] mPixelBufferRow;
        }

        void resizePixelBuffer(int width, int height)
        {
            if (mPixelBufferWidth != width || mPixelBufferHeight != height)
            {
                delete[] mPixelBuffer;
                mPixelBufferWidth = width;
                mPixelBufferHeight = height;
                mPixelBuffer = new unsigned char[mPixelBufferWidth * mPixelBufferHeight * mBufferDepth];
                memset(mPixelBuffer, 0xff, mPixelBufferWidth * mPixelBufferHeight * mBufferDepth);

                delete[] mPixelBufferRow;
                mPixelBufferRow = new unsigned char[mPixelBufferWidth * mBufferDepth];
            }
        }

        void copyPopupIntoView()
        {
            int popup_y = (mFlipYPixels ? (mPixelBufferHeight - mPopupBufferRect.y) : mPopupBufferRect.y);
            unsigned char* src = (unsigned char*)mPopupBuffer;
            unsigned char* dst = mPixelBuffer + popup_y * mPixelBufferWidth * mBufferDepth + mPopupBufferRect.x * mBufferDepth;
            while (src < (unsigned char*)mPopupBuffer + mPopupBufferRect.width * mPopupBufferRect.height * mBufferDepth)
            {
                memcpy(dst, src, mPopupBufferRect.width * mBufferDepth);
                src += mPopupBufferRect.width * mBufferDepth;
                dst += mPixelBufferWidth * mBufferDepth * (mFlipYPixels ? -1 : 1);
            }
        }


        bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override
        {
            mod_log("called GetViewRect");
            int width = 800;
            int height = 600;
            rect = CefRect(0, 0, width, height);
            return true;
        }

        void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override
        {
            mod_log("OnPaint");
            CEF_REQUIRE_UI_THREAD();

            resizePixelBuffer(width, height);

            // whole page was updated
            if (type == PET_VIEW)
            {
                // create (firs time) or resize (browser size changed) a buffer for pixels
                // and copy them in
                memcpy(mPixelBuffer, buffer, width * height * mBufferDepth);

                // we need to flip pixel buffer in Y direction as per settings
                if (mFlipYPixels)
                {
                    const size_t stride = mPixelBufferWidth * mBufferDepth;
                    unsigned char* lower = mPixelBuffer;
                    unsigned char* upper = mPixelBuffer + (mPixelBufferHeight - 1) * stride;
                    while (lower < upper)
                    {
                        memcpy(mPixelBufferRow, lower, stride);
                        memcpy(lower, upper, stride);
                        memcpy(upper, mPixelBufferRow, stride);
                        lower += stride;
                        upper -= stride;
                    }
                }

                // if there is still a popup open, write it into the page too (it's pixels will have been
                // copied into it's buffer by a call to OnPaint with type of PET_POPUP earlier)
                if (mPopupBuffer != nullptr)
                {
                    copyPopupIntoView();
                }
            }
            // popup was updated
            else if (type == PET_POPUP)
            {
                // copy over the popup pixels into it's buffer
                // (popup buffer created in onPopupSize() as we know the size there)
                memcpy(mPopupBuffer, buffer, width * height * mBufferDepth);

                // copy over popup pixels into page pixels. We need this for when popup is changing (e.g. highlighting or scrolling)
                // when the containing page is not changing and therefore doesn't get an OnPaint update
                copyPopupIntoView();
            }

            // if we have a buffer, indicate to consuming app that the page changed.
            if (mPixelBufferWidth > 0 && mPixelBufferHeight > 0)
            {
                mod_log("OnPaint() for size: %d x %d", width, height);
                // zpl_mutex_lock(&mod.mutexes.cef);

                if (g_texture == -1) {
                    return;
                }

                gfx_update_texture(g_texture, mPixelBuffer, mPixelBufferWidth, mPixelBufferHeight);

                // // RECT rect  = {x,y,z,w};  // the dimensions you want to lock
                // D3DLOCKED_RECT lockedRect = {0}; // "out" parameter from LockRect function below
                // g_texture->LockRect(0, &lockedRect, NULL, 0);

                // // copy the memory into lockedRect.pBits
                // // make sure you increment each row by "Pitch"

                // unsigned char* bits = ( unsigned char* )lockedRect.pBits;
                // // for( int row = 0; row < numRows; row++ )
                // // {
                // //     // copy one row of data into "bits", e.g. memcpy( bits, srcData, size )
                // //     ...

                // //     // move to the next row
                // //     bits += lockedRect.Pitch;
                // // }

                // memcpy(bits, mPixelBuffer, mPixelBufferWidth * mPixelBufferHeight * mBufferDepth);

                // // unlock when done
                // g_texture->UnlockRect(0);

                // zpl_mutex_unlock(&mod.mutexes.cef);

                // mParent->getCallbackManager()->onPageChanged(mPixelBuffer, 0, 0, mPixelBufferWidth, mPixelBufferHeight);
            }
        }

        // CefRenderHandler override
        void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override
        {
            CEF_REQUIRE_UI_THREAD();

            mod_log("Popup state set to %d", show);
            if (!show)
            {
                delete[] mPopupBuffer;
                mPopupBuffer = nullptr;

                mPopupBufferRect.Reset();
            }
        }

        // CefRenderHandler override
        void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override
        {
            CEF_REQUIRE_UI_THREAD();

            mPopupBufferRect = rect;
            if (mPopupBuffer == nullptr)
            {
                mPopupBuffer = new unsigned char[rect.width * rect.height * mBufferDepth];
                memset(mPopupBuffer, 0xff, rect.width * rect.height * mBufferDepth);
            }
        }


        IMPLEMENT_REFCOUNTING(RenderHandler);
};

class BrowserClient :
    public CefClient,
    public CefLifeSpanHandler
{
    public:
        BrowserClient(RenderHandler* render_handler) :
            render_handler_(render_handler)
        {
        }

        CefRefPtr<CefRenderHandler> GetRenderHandler() override
        {
            return render_handler_;
        }

        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
        {
            return this;
        }

        void OnAfterCreated(CefRefPtr<CefBrowser> browser) override
        {
            CEF_REQUIRE_UI_THREAD();

            browser_list_.push_back(browser);
        }

        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
        {
            CEF_REQUIRE_UI_THREAD();

            BrowserList::iterator bit = browser_list_.begin();
            for (; bit != browser_list_.end(); ++bit)
            {
                if ((*bit)->IsSame(browser))
                {
                    browser_list_.erase(bit);
                    break;
                }
            }

            if (browser_list_.empty())
            {
                // gExitFlag = true;
            }
        }


        void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
            mod_log("OnLoadEnd(%d)", httpStatusCode);
        }

        bool OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefLoadHandler::ErrorCode errorCode, const CefString & failedUrl, CefString & errorText) {
            mod_log("OnLoadError()");
        }

        void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) {
            mod_log("OnLoadingStateChange()");
        }

        void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
            mod_log("OnLoadStart()");
        }

        bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                           const CefString& target_url, const CefString& target_frame_name,
                           CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                           bool user_gesture, const CefPopupFeatures& popupFeatures,
                           CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client,
                           CefBrowserSettings& settings, bool* no_javascript_access)
        {
            mod_log("Page wants to open a popup");

            return true;
        };

        IMPLEMENT_REFCOUNTING(BrowserClient);

    private:
        CefRefPtr<CefRenderHandler> render_handler_;
        typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
        BrowserList browser_list_;
};

class CefMinimal : public CefApp
{
    public:
        ~CefMinimal()
        {
        }

        bool init(int argc, char* argv[])
        {
            CefSettings settings;
            CefMainArgs args(GetModuleHandle(nullptr));

            std::string path(platform_path());

            CefString(&settings.resources_dir_path)     = path + "\\cef";
            CefString(&settings.log_file)               = path + "\\cef\\ceflog.txt";
            CefString(&settings.locales_dir_path)       = path + "\\cef\\locales";
            CefString(&settings.cache_path)             = path + "\\cef\\cache";
            CefString(&settings.browser_subprocess_path)= path + "\\cef_worker.exe";

            settings.single_process                 = true;
            settings.multi_threaded_message_loop    = false;
            settings.log_severity                   = LOGSEVERITY_WARNING; // LOGSEVERITY_ERROR;
            settings.remote_debugging_port          = 7777;
            settings.windowless_rendering_enabled   = true;

            if (CefInitialize(args, settings, this, nullptr)) {
                mod_log("CefMinimal initialized okay");

                render_handler_ = new RenderHandler();
                browser_client_ = new BrowserClient(render_handler_);
                HWND win_id = (HWND)platform_windowid();

                CefWindowInfo window_info;
                window_info.SetAsWindowless(win_id);

                CefBrowserSettings browser_settings;
                browser_settings.windowless_frame_rate = 60;
                browser_settings.background_color = 0xffffffff;

                CefString url = "https://google.com/";
                browser_ = CefBrowserHost::CreateBrowserSync(window_info, browser_client_.get(), url, browser_settings, nullptr);

                return true;
            }

            mod_log("Unable to initialize");
            return false;
        }

        void navigate(const std::string url)
        {
            if (browser_.get() && browser_->GetMainFrame())
            {
                mod_log("CefMinimal loading URL %s", url);
                browser_->GetMainFrame()->LoadURL(url);
            }
        }

        void clickCenter()
        {
            // send to CEF
            if (browser_ && browser_->GetHost())
            {
                CefMouseEvent cef_mouse_event;
                cef_mouse_event.x = 512;
                cef_mouse_event.y = 512;
                cef_mouse_event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

                bool is_up = false;
                int last_click_count = 1;
                browser_->GetHost()->SendMouseClickEvent(cef_mouse_event, MBT_LEFT, is_up, last_click_count);

                is_up = true;
                browser_->GetHost()->SendMouseClickEvent(cef_mouse_event, MBT_LEFT, is_up, last_click_count);
            }
        }

        void update()
        {
            CefDoMessageLoopWork();
        }

        void requestExit()
        {
            if (browser_.get() && browser_->GetHost())
            {
                browser_->GetHost()->CloseBrowser(true);
            }
        }

        void shutdown()
        {
            render_handler_ = nullptr;
            browser_client_ = nullptr;
            browser_ = nullptr;
            CefShutdown();
        }

        IMPLEMENT_REFCOUNTING(CefMinimal);

    private:
        CefRefPtr<RenderHandler> render_handler_;
        CefRefPtr<BrowserClient> browser_client_;
        CefRefPtr<CefBrowser> browser_;
};

// =======================================================================//
// !
// ! Internal CEF implementation
// !
// =======================================================================//

CefRefPtr<CefMinimal> cm;
bool foobar = 0;

int cef_init() {
    cm = new CefMinimal();
    cm->init(0, nullptr);

    return 0;
}

cef_handle cef_browser_create(const char *url, int w, int h) {
    g_texture = gfx_create_texture(w, h);
    gfx_render_add(g_texture, CEF_DEFAULT_GFX_LAYER);

    cm->navigate(url);

    return 0;
}

int cef_tick() {
    if (cm) cm->update();
    return 0;
}