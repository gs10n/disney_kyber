#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define EM4305_WORDS 16
#define PROFILE_COUNT 12

typedef enum {
    DisneyKyberViewMain,
    DisneyKyberViewSelect,
} DisneyKyberView;

typedef struct {
    const char* name;
    uint32_t words[EM4305_WORDS];
} DisneyKyberProfile;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    Submenu* select_menu;

    uint32_t current_words[EM4305_WORDS];
    uint8_t selected_profile;
    uint8_t word_offset;
    char status_line[48];
} DisneyKyberApp;

static const DisneyKyberProfile disney_kyber_profiles[PROFILE_COUNT] = {
    {.name = "Luke Skywalker"},
    {.name = "Luminara Unduli"},
    {.name = "Plo Koon"},
    {.name = "Ben Solo"},
    {.name = "Mace Windu"},
    {.name = "General Grievous"},
    {.name = "Rey Skywalker"},
    {.name = "Krin Dagbard"},
    {.name = "Grand Inquisitor"},
    {.name = "Maul"},
    {.name = "Asajj Ventress"},
    {.name = "Darth Sidious"},
};

static bool disney_kyber_read_words(uint32_t out_words[EM4305_WORDS]) {
    furi_check(out_words);

    /*
       Hook this function up to the EM4305 helpers from flipperzero-firmware.
       The app flow is ready; only the low-level read transport should be swapped
       for the concrete firmware API calls.
    */
    return false;
}

static bool disney_kyber_write_words(const uint32_t in_words[EM4305_WORDS]) {
    furi_check(in_words);

    /*
       Hook this function up to the EM4305 helpers from flipperzero-firmware.
       This wrapper is called from the main scene's Write action.
    */
    return false;
}

static void disney_kyber_set_status(DisneyKyberApp* app, const char* text) {
    strlcpy(app->status_line, text, sizeof(app->status_line));
}

static void disney_kyber_main_draw(Canvas* canvas, void* context) {
    DisneyKyberApp* app = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Disney Kyber");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 19, "L:Read  OK:Select  R:Write");
    canvas_draw_str(canvas, 2, 27, "U/D:Scroll 16 words");
    canvas_draw_str(canvas, 2, 35, app->status_line);

    char line[32];
    snprintf(line, sizeof(line), "Profile: %s", disney_kyber_profiles[app->selected_profile].name);
    canvas_draw_str(canvas, 2, 43, line);

    for(uint8_t row = 0; row < 3; row++) {
        uint8_t idx = app->word_offset + row;
        if(idx >= EM4305_WORDS) break;

        snprintf(line, sizeof(line), "%02u: %08lX", idx, app->current_words[idx]);
        canvas_draw_str(canvas, 2, 51 + (row * 7), line);
    }
}

static void disney_kyber_on_read(DisneyKyberApp* app) {
    if(disney_kyber_read_words(app->current_words)) {
        disney_kyber_set_status(app, "Read successful");
        app->word_offset = 0;
    } else {
        disney_kyber_set_status(app, "Read failed");
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static void disney_kyber_on_write(DisneyKyberApp* app) {
    if(disney_kyber_write_words(disney_kyber_profiles[app->selected_profile].words)) {
        disney_kyber_set_status(app, "Write successful");
    } else {
        disney_kyber_set_status(app, "Write failed");
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static bool disney_kyber_main_input(InputEvent* event, void* context) {
    DisneyKyberApp* app = context;

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyLeft) {
        disney_kyber_on_read(app);
        return true;
    }

    if(event->key == InputKeyOk) {
        view_dispatcher_switch_to_view(app->view_dispatcher, DisneyKyberViewSelect);
        return true;
    }

    if(event->key == InputKeyRight) {
        disney_kyber_on_write(app);
        return true;
    }

    if(event->key == InputKeyUp) {
        if(app->word_offset > 0) app->word_offset--;
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        return true;
    }

    if(event->key == InputKeyDown) {
        if(app->word_offset < (EM4305_WORDS - 3)) app->word_offset++;
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        return true;
    }

    return false;
}

static void disney_kyber_profile_select_cb(void* context, uint32_t index) {
    DisneyKyberApp* app = context;
    app->selected_profile = index;
    disney_kyber_set_status(app, "Profile selected");
    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyKyberViewMain);
}

static bool disney_kyber_custom_event_cb(void* context, uint32_t event) {
    UNUSED(context);
    UNUSED(event);
    return true;
}

static bool disney_kyber_back_event_cb(void* context) {
    DisneyKyberApp* app = context;

    if(view_dispatcher_get_current_view(app->view_dispatcher) == DisneyKyberViewSelect) {
        view_dispatcher_switch_to_view(app->view_dispatcher, DisneyKyberViewMain);
        return true;
    }

    return false;
}

static DisneyKyberApp* disney_kyber_app_alloc(void) {
    DisneyKyberApp* app = malloc(sizeof(DisneyKyberApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->main_view = view_alloc();
    app->select_menu = submenu_alloc();

    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, disney_kyber_main_draw);
    view_set_input_callback(app->main_view, disney_kyber_main_input);

    for(uint32_t i = 0; i < PROFILE_COUNT; i++) {
        submenu_add_item(app->select_menu, disney_kyber_profiles[i].name, i, disney_kyber_profile_select_cb, app);
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, disney_kyber_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, disney_kyber_back_event_cb);

    view_dispatcher_add_view(app->view_dispatcher, DisneyKyberViewMain, app->main_view);
    view_dispatcher_add_view(
        app->view_dispatcher,
        DisneyKyberViewSelect,
        submenu_get_view(app->select_menu));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    memset(app->current_words, 0, sizeof(app->current_words));
    app->selected_profile = 0;
    app->word_offset = 0;
    disney_kyber_set_status(app, "Ready");

    return app;
}

static void disney_kyber_app_free(DisneyKyberApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, DisneyKyberViewMain);
    view_dispatcher_remove_view(app->view_dispatcher, DisneyKyberViewSelect);

    submenu_free(app->select_menu);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t disney_kyber_app(void* p) {
    UNUSED(p);

    DisneyKyberApp* app = disney_kyber_app_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyKyberViewMain);
    view_dispatcher_run(app->view_dispatcher);
    disney_kyber_app_free(app);

    return 0;
}
