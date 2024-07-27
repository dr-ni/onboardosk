#!/usr/bin/gjs

const GtkClutter = imports.gi.GtkClutter;
const Clutter = imports.gi.Clutter;
const OnboardOsk = imports.gi.OnboardOsk;
const Lang = imports.lang;
const Signals = imports.signals;
const GLib = imports.gi.GLib;


const Rect = new Lang.Class({
    Name: 'Rect',

    _init: function(x, y, width, height) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
    },
});

// Mock gnome-shell St module
var St = {
    DrawingArea: new Lang.Class({
        Name: 'DrawingArea',
        Extends: Clutter.Actor,
        Implements: [Signals.WithSignals],

        _init: function() {
            this.parent();
            this._content = new Clutter.Canvas();
            this.set_content(this._content);
            this.connect('allocation-changed',
                         Lang.bind(this, this._onAllocationChanged));

            //this._content.connect('draw',
            //             Lang.bind(this, this._onDraw));
        },

        _onAllocationChanged: function() {
            let w, h;
            [w, h] = this.get_size();
            this._content.set_size(w, h);
            log('_onAllocationChanged', w, h);
        },

        _onDraw: function(canvas, cr) {
            this._cr = cr;
            this._emitRepaint();
            this._cr = null;
            cr.$dispose();
        },

        get_context: function() {
            return this._cr;
        },

        _emitRepaint: function() {
            this.emit('repaint');
        },

       // Signals.addSignalMethods doesn't work. It can't
       // delegte to parent, apparently.
       // Do connect and emit by hand instead. It's just a
       // mock class anyway.
       connect: function(name, callback)
        {
            if (name == 'repaint')
            {
                this.repaint_callback = callback;
               return 0;
            }
            this.parent(name, callback);
        },

        emit: function(name)
        {
            if (name == 'repaint')
            {
                if (this.repaint_callback)
                    this.repaint_callback();
                return;
            }
            this.parent(name);
        },
    }),
};

const Monitor = new Lang.Class({
    Name: 'Monitor',

    _init: function(index, geometry) {
        this.index = index;
        this.x = geometry.x;
        this.y = geometry.y;
        this.width = geometry.width;
        this.height = geometry.height;
    },
});

const MockLayoutManager = new Lang.Class({
    Name: 'MockLayoutManager',

    _init: function() {
        Clutter.init(null, 0);

        //Create some colors from Clutter Color pattern
        let stage_bg_color =
            Clutter.Color.get_static(Clutter.StaticColor.CHOCOLATE_DARK);
        let actor_bg_color_plum =
            Clutter.Color.get_static(Clutter.StaticColor.PLUM);
        let actor_bg_color_red =
            Clutter.Color.get_static(Clutter.StaticColor.RED);
        let gray = Clutter.Color.get_static(Clutter.StaticColor.GRAY);
        //let black = new Clutter.Color( {red:0, blue:0, green:0, alpha:255} );

        //Create Stage
        // deprecated and stuck at fixed window size in wayland
        // let stage = Clutter.Stage.get_default();
        let stage = new Clutter.Stage();
        if (0)
        {
            stage.set_position(0, 0);
            stage.set_size(1980, 1200);
        }
        else
        {
            //stage.set_position(579, 0);
            stage.set_size(1400, 600);
            stage.set_position(0, 0);
        }
        stage.set_user_resizable(true);
        stage.set_title('Onboard Test Stage');
        stage.set_background_color(stage_bg_color);

        let button;

        // button to hide/show the keyboard
        button = new Clutter.Text();
        button.set_activatable(false);
        button.set_editable(false);
        button.set_cursor_visible(false);
        button.set_text('visible');
        button.set_position(20, 20);
        button.set_size(100, 50);
        button.set_background_color(actor_bg_color_plum);
        button.set_reactive(true);

        function on_toggle_visible_button_press(stage, event) {
            print('press', onboardosk.is_visible());
            onboardosk.toggle_visible();
            return false;
        }
        button.connect('button-press-event', on_toggle_visible_button_press);
        stage.add_child(button);

        // button to simulate changing system keyboard layout
        button = new Clutter.Text();
        button.set_activatable(false);
        button.set_editable(false);
        button.set_cursor_visible(false);
        button.set_text('input source');
        button.set_position(20, 90);
        button.set_size(100, 50);
        button.set_background_color(actor_bg_color_plum);
        button.set_reactive(true);

        function on_input_source_button_press(stage, event) {
            onboardosk.input_source_index++;
            onboardosk.on_group_changed();
            return false;
        }
        button.connect('button-press-event', on_input_source_button_press);
        stage.add_child(button);

        // button to destroy the keyboard including all actors
        button = new Clutter.Text();
        button.set_activatable(false);
        button.set_editable(false);
        button.set_cursor_visible(false);
        button.set_text('destroy');
        button.set_position(20, 160);
        button.set_size(100, 50);
        button.set_background_color(actor_bg_color_plum);
        button.set_reactive(true);

        function on_destroy_button_press(stage, event) {
            onboardosk.destroy();
            onboardosk = null;
            return false;
        }
        button.connect('button-press-event', on_destroy_button_press);
        stage.add_child(button);

        let tick_mark;
        let ypos = 220;
        let xstep = 100;
        let w = 30;
        for (let i=0; i<20; i++) {
            tick_mark = new Clutter.Text();
            tick_mark.set_activatable(false);
            tick_mark.set_editable(false);
            tick_mark.set_cursor_visible(false);
            tick_mark.set_text((i * xstep).toString());
            tick_mark.set_position(i * xstep, ypos);
            tick_mark.set_size(w, 20);
            tick_mark.set_background_color(gray);
            stage.add_child(tick_mark);
        }

        print('test_stage: showing stage');
        stage.show();
        stage.connect('destroy', Clutter.main_quit);

        this.stage = stage;

        this.monitors = [new Monitor(0, new Rect(0, 0, 1920, 1200))];
        this.primaryIndex = 8;

    },

    getWorkAreaForMonitor: function(index) {
        let m = this.monitors[index];
        return new Rect(m.x, m.y, m.width, m.height);
    },

    findIndexForActor: function(actor) {
        return 0;
    },

    focusIndex: function() {
        return 0;
    },

    addChrome: function(actor) {
        this.stage.add_child(actor);
    },
});

const MockUIGroup = new Lang.Class({
    Name: 'uiGroup',

    _init: function() {
    },

    add_actor: function(actor) {
    },
});

// Mock gnome-shell Main module
var Main = {
    layoutManager: new MockLayoutManager(),

    uiGroup: new MockUIGroup(),
};


const MockScreen = new Lang.Class({
    Name: 'MockScreen',

    _init: function() {
    },

    connect: function(signal, func) {
        return 0;
    },

    disconnect: function(id) {
    },

    set_cursor: function() {
    },
});

// Mock gnome-shell global variable
var global = {
    screen: new MockScreen(),
    stage: Main.layoutManager.stage,
};

const InputSource = new Lang.Class({
    Name: 'InputSource',

    _init: function(index, id, type) {
        this.index = index;
        this.id = id;
        this.type = type;
    },
});

var InputSourceManager = {
    INPUT_SOURCE_TYPE_XKB: 'xkb',
    INPUT_SOURCE_TYPE_IBUS: 'ibus',

    MockInputSourceManager: new Lang.Class({
        Name: 'MockInputSourceManager',

        _init: function() {
        },

        connect: function() {
        },
    }),

    getInputSourceManager: function() {
        let m = new this.MockInputSourceManager();
        m.inputSources = [new InputSource(0, 'us', this.INPUT_SOURCE_TYPE_XKB),
                          new InputSource(1, 'de', this.INPUT_SOURCE_TYPE_XKB)];
        m.currentSource = m.inputSources[0];
        return m;
    },
};

const MockXkbInfo = new Lang.Class({
    Name: 'MockXkbInfo',

    _init: function() {
    },
    get_layout_info: function() {
        return ['', '', '', 'us', 'cyrillic'];
    },
});

var KeyboardManager = {

    MockKeyboardManager: new Lang.Class({
        Name: 'MockKeyboardManager',

        MAX_LAYOUTS_PER_GROUP: 3,

        _init: function() {
        },

    }),

    getKeyboardManager: function() {
        return new this.MockKeyboardManager();
    },

    getXkbInfo: function() {
        return new MockXkbInfo();
    },
};

const MockVirtualKeyboardDevice = new Lang.Class({
    Name: 'MockVirtualKeyboardDevice',

    _init: function() {
        this.parent();
    },

    notify_key: function() {
    },

    notify_keyval: function() {
    },
});

const MockKeyState = new Lang.Class({
    Name: 'MockKeyState',
    PRESSED: 0,
    RELEASED: 1,

});

Clutter.KeyState = new MockKeyState();

function create_virtual_keyboard_device() {
    return new MockVirtualKeyboardDevice();
}

const MockPopupMenuManager = new Lang.Class({
    Name: 'MockPopupMenuManager',

    _init: function() {
    },

    addMenu: function(menu) {
    },
});

var PopupMenu = {
    PopupMenuManager: new Lang.Class({
        Name: 'PopupMenuManager',

        _init: function(actor) {
            this.parent();
        },

        addMenu: function(menu) {
        },
    }),
};


function log(...args) {
    print(...args);
}

// Actor for Onboard's toplevel view
const ToplevelActorSt = new Lang.Class({
    Name: 'ToplevelActorSt',
    Extends: St.DrawingArea,

    _init: function() {
        this.parent();
        this.view = null;

        this.connect('repaint',
                     Lang.bind(this, this._onRepaint));
    },

    _onRepaint: function() {
        let cr = this.get_context();

        if (0)
        {
            let color = Clutter.Color.get_static(Clutter.StaticColor.RED);
            let x, y, w, h;
            [x, y] = this.get_position();
            [w, h] = this.get_size();
            log('_onRepaint', w, h);

            Clutter.cairo_set_source_color(cr, color);
            cr.rectangle(0, 0, w, h);
            cr.fill();
        }
        this.view.draw(cr);
    },
});

const ToplevelActor = ToplevelActorSt;



const LanguageMenu = new Lang.Class({
    Name: 'LanguageMenu',

    _init: function(sourceActor) {
        this.actor = 0;
    },

    popup: function(instance, actor, rect,
                    active_lang_id, system_lang_id,
                    mru_lang_ids, other_lang_ids) {
        log('LanguageMenu.populate',
            active_lang_id, system_lang_id,
            mru_lang_ids, other_lang_ids);
        log('LanguageMenu.show');
        GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                         1000,
                         Lang.bind(this, function() {
                            instance.on_language_selection_closed();
                            return GLib.SOURCE_REMOVE;
                         }));
    },
});


// Derived class of Onboard's Instance
const LocalInstance = new Lang.Class({
    Name: 'LocalInstance',
    Extends: OnboardOsk.Instance,

    _init: function() {
        this.parent();
        this._virtualDevice = create_virtual_keyboard_device();

        this._monitorsChangedId =
            global.screen.connect('monitors-changed',
                                  Lang.bind(this, this._onMonitorsChanged));

        this._inputSourceManager = InputSourceManager.getInputSourceManager();
        this._currentSourceId =
            this._inputSourceManager.connect(
                'current-source-changed',
                Lang.bind(this, this._onSourceChanged));
        this._sourcesChangtedId =
            this._inputSourceManager.connect(
                'sources-changed',
                Lang.bind(this, this._onSourcesModified));

        this._keyboardManager = KeyboardManager.getKeyboardManager();
        this._xkbInfo = KeyboardManager.getXkbInfo();

        this._languageMenu = new LanguageMenu();

        this._actors = {};

        this._grab_actor = null;
        this._init_grab();
    },

    destroy: function() {
        log('LocalInstance.destroy');
        if (this._currentSourceId)
            this._inputSourceManager.disconnect(this._currentSourceId);
        if (this._sourcesChangtedId)
            this._inputSourceManager.disconnect(this._sourcesChangtedId);
        if (this._monitorsChangedId)
            global.screen.disconnect(this._monitorsChangedId);

        if (this._languageMenu)
            this._languageMenu.destroy();
        this._languageMenu = null;

        if (this._grab_actor)
        {
            this._grab_actor.destroy();
            this._grab_actor = null;
        }

        this.parent();
    },

    vfunc_on_toplevel_view_added: function(view) {
        log('LocalInstance.vfunc_on_toplevel_view_added');
        let actor = new ToplevelActor();
        actor.view = view;
        view.set_actor(actor);
        this._actors[view.get_name()] = actor;

        if (view.is_reactive_type())
        {
            actor.set_reactive(true);
        }
        else
        {
            actor.set_reactive(false);
        }
        Main.layoutManager.addChrome(actor);

        view.connect('cursor-change-request',
                      Lang.bind(this, this._on_cursor_change_request));
    },

    vfunc_on_toplevel_view_remove: function(view) {
        let actor = view.get_actor();
        actor.view = null;
        delete this._actors[view.get_name()];
        actor.destroy();
    },

    _on_cursor_change_request: function(actor, cursor_type) {
        log('LocalInstance.on_cursor_change_request', cursor_type);
        global.screen.set_cursor(cursor_type);
    },

    _get_actor_of_view: function(view) {
        // view.get_actor() only returns a GObject pointer which isn't useful
        // beyond calling destroy(). This function returns the real
        // ClutterActor.
        return this._actors[view.get_name()];
    },

    _onMonitorsChanged: function() {
    },

    vfunc_get_n_monitors: function() {
        return Main.layoutManager.monitors.length;
    },

    vfunc_get_monitor_geometry: function(index) {
        let m = Main.layoutManager.monitors[index];
        log('vfunc_get_monitor_geometry', [m.x, m.y, m.width, m.height]);
        return [m.x, m.y, m.width, m.height];
    },

    vfunc_get_monitor_size_mm: function(index) {
        // Can't access mutter's meta_monitor_get_physical_dimensions()?
        return [0.0, 0.0];
    },

    vfunc_get_monitor_workarea: function(index) {
        let wa = Main.layoutManager.getWorkAreaForMonitor(index);
        log('vfunc_get_monitor_workarea', [wa.x, wa.y, wa.width, wa.height]);
        return [wa.x, wa.y, wa.width, wa.height];
    },

    vfunc_get_monitor_at_active_window: function() {
        let index = Main.layoutManager.focusIndex();
        log('vfunc_get_monitor_at_active_window', index);
        return index;
    },

    vfunc_get_monitor_at_actor: function(actor) {
        let index = Main.layoutManager.findIndexForActor(actor);
        log('vfunc_get_monitor_at_actor', index);
        return index;
    },

    vfunc_get_primary_monitor: function() {
        let index = Main.layoutManager.primaryIndex;
        log('vfunc_get_primary_monitor', index);
        return index;
    },

    // One of GNOME Shell's keyboard input source was modified
    _onSourcesModified: function() {
        log('LocalInstance._onSourcesModified');
        this.on_group_changed();
    },

    // GNOME Shell's active keyboard input source changed
    _onSourceChanged: function(inputSourceManager, oldSource) {
        log('LocalInstance._onSourcesChanged');
        let source = inputSourceManager.currentSource;
        log(source.index, source.id, source.xkbId,
            source.type, source.displayName);
        let info = this._xkbInfo.get_layout_info(source.id);
        log(info);
        this.on_group_changed();
    },

    vfunc_get_current_group: function() {
        // Groups are:
        // source.index % (this._keyboardManager.MAX_LAYOUTS_PER_GROUP - 1),
        // unless there are IBus input sources in between. These have
        // to be skipped.
        let currentSource = this._inputSourceManager.currentSource;
        let xkbCount = 0;
        let inputSources = this._inputSourceManager.inputSources;
        for (let i in inputSources) {
            if (i > currentSource.index)
                break;
            let source = inputSources[i];
            if (source.type == InputSourceManager.INPUT_SOURCE_TYPE_XKB)
                xkbCount++;
        }
        let groupIndex = (xkbCount - 1) %
                         (this._keyboardManager.MAX_LAYOUTS_PER_GROUP - 1);

        log('LocalInstance.vfunc_get_current_group', groupIndex);
        return groupIndex;
    },

    vfunc_get_current_rules_names: function() {
        let source = this._inputSourceManager.currentSource;
        let layouts = '';
        let variants = '';

        if (source.type == InputSourceManager.INPUT_SOURCE_TYPE_XKB) {
            [, , , layouts, variants] =
                this._xkbInfo.get_layout_info(source.id);
        } else if (source.type == InputSourceManager.INPUT_SOURCE_TYPE_IBUS) {
            let engineDesc =
                IBusManager.getIBusManager().getEngineDesc(source.id);
            if (engineDesc) {
                layouts = engineDesc.get_layout();
                variants = engineDesc.get_layout_variant();
            }
        }
        let names = ['', '', layouts, variants, ''];
        log('LocalInstance.vfunc_get_rules_names', names);
        return [names, names.length];
    },

    vfunc_send_keyval_event: function(keyval, press) {
        let state = press ? Clutter.KeyState.PRESSED :
                            Clutter.KeyState.RELEASED;
        log('LocalInstance.send_keyval_event ', keyval, press, state);
        this._virtualDevice.notify_keyval(Clutter.get_current_event_time(),
                                          keyval, state);
    },

    vfunc_send_keycode_event: function(keycode, press) {
        let state = press ? Clutter.KeyState.PRESSED :
                            Clutter.KeyState.RELEASED;
        log('LocalInstance.send_keycode_event ', keycode, press, state);
        this._virtualDevice.notify_key(Clutter.get_current_event_time(),
                                       keycode, state);
    },

    // Create an invisible actor covering the whole stage. It only
    // becomes visible (reactive isn't enough) when a 'grab' is
    // initiated with vfunc_grab_pointer().
    // Z-order doesn't matter as the onboard instance requests a clutter grab.
    // We just need to make sure all the stage is covered with actors.
    _init_grab: function() {
        log('LocalInstance.vfunc_grab_pointer');
        let color = new Clutter.Color({red: 255, blue: 0, green: 0,
                                       alpha: 32});
        let grab_actor = new Clutter.Actor();
        grab_actor.set_position(10, 10);
        grab_actor.set_size(1000, 500);
        grab_actor.set_background_color(color);
        grab_actor.set_x_expand(true);
        grab_actor.set_y_expand(true);
        grab_actor.set_reactive(true);
        //grab_actor.set_opacity(0);  // make it invisible
        let constraint = new Clutter.BindConstraint(
            {source: global.stage,
             coordinate: Clutter.BindCoordinate.ALL});
        grab_actor.add_constraint(constraint);
        grab_actor.hide(true);

        // Allow to easily stop the 'grab' if anything goes wrong.
        function on_grab_actor_button_press(actor, event) {
            print('LocalInstance.on_grab_actor_button_press', event.type);
            vfunc_ungrab_pointer();
            return false;
        }
        function on_grab_actor_touch(actor, event) {
            print('LocalInstance.on_grab_actor_touch', event.type);
            if (event.type() == Clutter.EventType.TOUCH_BEGIN)
                vfunc_ungrab_pointer();
            return false;
        }
        grab_actor.connect('button-press-event', on_grab_actor_button_press);
        grab_actor.connect('touch-event', on_grab_actor_touch);

        Main.layoutManager.addChrome(grab_actor);

        this._grab_actor = grab_actor;
    },

    vfunc_grab_pointer: function() {
        log('LocalInstance.vfunc_grab_pointer');
        if (this._grab_actor)
            this._grab_actor.show();
    },

    vfunc_ungrab_pointer: function() {
        log('LocalInstance.vfunc_ungrab_pointer');
        if (this._grab_actor)
            this._grab_actor.hide();
    },

    vfunc_show_language_selection: function(view, rect,
                                    active_lang_id, system_lang_id,
                                    mru_lang_ids, other_lang_ids) {
        log('LocalInstance.vfunc_select_language');
        let actor = this._get_actor_of_view(view);

        this._languageMenu.popup(this, actor, rect,
                                 active_lang_id, system_lang_id,
                                 mru_lang_ids, other_lang_ids);
    },
});





let onboardosk = new LocalInstance();
onboardosk.start();

print('Clutter.main()');
Clutter.main();
