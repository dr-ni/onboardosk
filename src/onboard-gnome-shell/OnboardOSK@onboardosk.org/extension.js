/*
 * Copyright © 2016 marmuta <marmvta@gmail.com>
 * Copyright © 2016 Simon Schumann
 *
 * DBus proxy and default keyboard hiding based on ideas by Simon Schumann.
 * https://github.com/schuhumi/gnome-shell-extension-onboard-integration
 *
 * This file is part of Onboard.
 *
 * Onboard is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.

 * Onboard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

const Config = imports.misc.config;

imports.gi.versions.Clutter = Config.LIBMUTTER_API_VERSION;

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Main = imports.ui.main;
const Mainloop = imports.mainloop;
const Tweener = imports.ui.tweener;
const Keyboard = imports.ui.keyboard.Keyboard;
const BoxPointer = imports.ui.boxpointer;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Util = imports.misc.util;

const InputSourceManager = imports.ui.status.keyboard;
const KeyboardManager = imports.misc.keyboardManager;

const OnboardOsk = imports.gi.OnboardOsk;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

let _oskManager;


function create_virtual_keyboard_device() {
    let deviceManager = Clutter.DeviceManager.get_default();
    return deviceManager.create_virtual_device(
        Clutter.InputDeviceType.KEYBOARD_DEVICE);
}

function apply_transform_to_rect(actor, rect) {
        let v = new Clutter.Vertex({x: rect[0], y: rect[1]});
        let vb = actor.apply_transform_to_point(v);
        v.x = rect[0] + rect[2];
        v.y = rect[1] + rect[3];
        let ve = actor.apply_transform_to_point(v);
        return [vb.x, vb.y, ve.x - vb.x, ve.y - vb.y];
}


const LanguageMenu = new Lang.Class({
    Name: 'LanguageMenu',

    _init: function() {
        this.parent();

        this._sourceActor = null;
        this._popupMenuManager = null;
        this._popupMenu = null;
        this._menuClosedId = null;
        this._actorDestroyId = null;
    },

    popup: function(instance, actor, rect,
                    active_lang_id, system_lang_id,
                    mru_lang_ids, other_lang_ids) {

        this._sourceActor = actor;

        rect = apply_transform_to_rect(actor, rect);
        //let [x, y] = actor.get_transformed_position();
        //let [w, h] = actor.get_transformed_size();
        //x = (rect[0] + rect[2] / 2.0) - w / 2.0;
        //rect = [x, y, w, h];

        this.destroy();
        this._popupMenu = new PopupMenu.PopupMenu(
            Main.layoutManager.dummyCursor, 0.5, St.Side.BOTTOM);
        this._populate(this._popupMenu, instance,
                       active_lang_id, system_lang_id,
                       mru_lang_ids, other_lang_ids);
        Main.uiGroup.add_actor(this._popupMenu.actor);
        this._popupMenu.actor.hide();  // no glitch appearance at 0, 0

        this._popupMenuManager = new PopupMenu.PopupMenuManager(
            {actor: actor});
        this._popupMenuManager.addMenu(this._popupMenu);

        this._menuClosedId = this._popupMenu.connect('menu-closed',
            Lang.bind(this, function() {
            log('LanguageMenu.create: menu closed');
            instance.on_language_selection_closed();
        }));

        this._actorDestroyId = actor.connect('destroy',
            Lang.bind(this, function() {
                log('LanguageMenu.create: actor destroyed');
                this._destroy();
        }));

        // Open the popup delayed to prevent interesting side effects
        // like it closing right away and the grab getting stuck.
        this._openPopupIdleId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE,
                      Lang.bind(this, function() {
                          this._idle_open_language_popup(rect);
                        return GLib.SOURCE_REMOVE;
                      }));
        GLib.Source.set_name_by_id(this._openPopupIdleId,
                                   '[Onboard_Indicator] open popup');
    },

    _idle_open_language_popup: function(rect) {
        Main.layoutManager.setDummyCursorGeometry(rect[0], rect[1],
                                                  rect[2], rect[3]);
        this._popupMenu.open(BoxPointer.PopupAnimation.NONE);
    },

    destroy: function() {
        if (this._menuClosedId)
            this._popupMenu.disconnect(this._menuClosedId);
        this._menuClosedId = null;

        if (this._sourceActor)
        {
            if (this._actorDestroyId)
                this._sourceActor.disconnect(this._actorDestroyId);
            this._actorDestroyId = null;
            this._sourceActor = null;
        }

        if (this._popupMenu)
            this._popupMenu.destroy();
        this._popupMenu = null;
        this._popupMenuManager = null;
    },

    _populate: function(menu, instance,
                        active_lang_id, system_lang_id,
                        mru_lang_ids, other_lang_ids) {
        menu.removeAll();

        // system language item
        let name = instance.get_language_full_name(system_lang_id);
        let item = menu.addAction(name, function() {
                instance.set_active_language_id('', false);
            });
        if (active_lang_id == '')
            item.setOrnament(PopupMenu.Ornament.CHECK);
        else
            item.setOrnament(PopupMenu.Ornament.NONE);

        menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        // MRU languages section
        for (let i = 0; i < mru_lang_ids.length; i++) {
            let lang_id = mru_lang_ids[i];
            let name = instance.get_language_full_name(lang_id);

            let item = menu.addAction(name, function() {
                instance.set_active_language_id(lang_id, false);
            });

            if (lang_id == active_lang_id)
                item.setOrnament(PopupMenu.Ornament.CHECK);
            else
                item.setOrnament(PopupMenu.Ornament.NONE);
        }

        // Other languages sub-menu
        if (mru_lang_ids.length)
            menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        if (other_lang_ids.length)
        {
            let submenu = new PopupMenu.PopupSubMenuMenuItem(
                _("Other Languages"));

            for (let i = 0; i < other_lang_ids.length; i++) {
                let lang_id = other_lang_ids[i];
                let name = instance.get_language_full_name(lang_id);

                submenu.menu.addAction(name, function() {
                    instance.set_active_language_id(lang_id, true);
                });
            }

            menu.addMenuItem(submenu);
        }
    },
});


// Actor for Onboard's toplevel view.
// Must be an St actor, else popup menus won't work.
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

        let w, h;
        [w, h] = this.get_size();
        //log('_onRepaint', w, h);

        /*
        let color = Clutter.Color.get_static(Clutter.StaticColor.RED);
        Clutter.cairo_set_source_color(cr, color);
        cr.rectangle(0, 0, w, h);
        cr.fill();
        */

        this.view.draw(cr);
    },
});

// Alternative actor for Onboard's toplevel view.
// Faster repainting? Maybe a little bit.
const ToplevelActorCanvas = new Lang.Class({
    Name: 'ToplevelActorCanvas',
    Extends: Clutter.Actor,

    _init: function() {
        this.parent();
        this.view = null;
        this._content = new Clutter.Canvas();
        this.set_content(this._content);
        this.connect('allocation-changed',
                     Lang.bind(this, this._onAllocationChanged));
    },

    _onAllocationChanged: function() {
        let w, h;
        [w, h] = this.get_size();
        this._content.set_size(w, h);
    },

});

const ToplevelActor = ToplevelActorSt;


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


const OskManager = new Lang.Class({
    Name: 'OskManager',

    _init: function() {
        this._oldKeyboardSync = null;
        this._onboardosk = null;
        this._focusNotifyId = null;

        this.enable();
        this._updateKeyboard();
    },

    destroy: function() {
        log('OskManager.destroy()');
        this.disable();
        this._destroyKeyboard();
    },

    enable: function() {
        function _sync(outer_this) {
            return (function(monitor) {
                if (this._keyboard)
                    this._destroyKeyboard();
            });
        };

        if (!this._oldKeyboardSync)
        {
            log('OSKManager.enable');

            this._oldKeyboardSync = Keyboard.prototype['_sync'];
            Keyboard.prototype['_sync'] = _sync(this);
            Main.keyboard._sync();
        }
    },

    disable: function() {
        if (this._oldKeyboardSync)
        {
            log('OSKManager.disable');
            Keyboard.prototype['_sync'] = this._oldKeyboardSync;
            this._oldKeyboardSync = null;
        }
    },

    toggleVisible: function() {
        this._updateKeyboard();
        this._onboardosk.toggle_visible();
    },

    _destroyKeyboard: function() {
        log('OSKManager._destroyKeyboard');

        if (this._focusNotifyId)
            global.stage.disconnect(this._focusNotifyId);

        // Destroying all onboard actors should automatically
        // remove them from the chrome (allowing access to
        // the screen below again).
        this._onboardosk.destroy();
        this._onboardosk = null;
    },

    // Notification of focus change for GNOME-Shell owned text entries (actors)
    _onKeyFocusChanged: function() {
        let focus = global.stage.key_focus;
        print('OSKManager._onKeyFocusChanged', focus);
    },

    _updateKeyboard: function() {
        if (!this._onboardosk)
        {
            this._createKeyboard();
        }
    },

    _createKeyboard: function() {
        log('OSKManager._createKeyboard');

        //Main.layoutManager.keyboardBox.add_actor(this.actor);
        //Main.layoutManager.trackChrome(this.actor);
        //Main.layoutManager._updateKeyboardBox();  // set pos and width

        this._onboardosk = new LocalInstance();
        this._onboardosk.start();

        //this._focusNotifyId = global.stage.connect('notify::key-focus',
        //        Lang.bind(this, this._onKeyFocusChanged));
    },
});


const OnboardIndicator = new Lang.Class({
    Name: 'OnboardIndicator',
    Extends: PanelMenu.Button,

    _init: function() {
        this.parent(0.0, _("Onboard"));

        this._last_event_time = 0;
        this._long_press_timer_id = 0;

        this._hbox = new St.BoxLayout({ style_class: 'panel-status-menu-box' });
        this._hbox.add_child(new St.Icon({ icon_name: 'onboardosk-symbolic',
                                           style_class: 'system-status-icon',
                                         }));
        this.actor.add_child(this._hbox);

        this.menu.addAction(_("Preferences"), function(event) {
            //GLib.spawn_command_line_async('onboard-settings', null);
            Util.spawn(['onboard-settings']);
        });

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this.menu.addAction(_("Help"), function(event) {
            //GLib.spawn_command_line_async('/usr/bin/yelp help:onboard', null);
            Util.spawn(['/usr/bin/yelp', 'help:onboard']);
        });
    },

    // Left-click/touch of the icon toggles visibility of the keyboard.
    // Clicking the icon with any other button or long-press opens the menu.
    _onEvent: function(actor, event) {
        if (event.type() == Clutter.EventType.TOUCH_BEGIN ||
            (event.type() == Clutter.EventType.BUTTON_PRESS &&
             event.get_button() == 1))
        {
            this.menu.close();

            // start long-press timer
            let settings = Clutter.Settings.get_default();
            let duration = settings.long_press_duration;
            this._long_press_timer_id = Mainloop.timeout_add(duration,
                Lang.bind(this, function() {
                    this._stop_long_press_timer();
                    this.menu.toggle();
                    return GLib.SOURCE_REMOVE;
                }));
            GLib.Source.set_name_by_id(this._long_press_timer_id,
                '[Onboard_Indicator] long_press');

            return Clutter.EVENT_PROPAGATE;
        }
        else if (event.type() == Clutter.EventType.TOUCH_END ||
            (event.type() == Clutter.EventType.BUTTON_RELEASE &&
             event.get_button() == 1))
        {
            // do nothing if long-press happened already
            if (this._long_press_timer_id == 0)
                return Clutter.EVENT_PROPAGATE;

            // else toggle the keyboard
            this._stop_long_press_timer();

            // TOUCH and BUTTON event may come together.
            // Act only on the first one.
            if (event.get_time() - this._last_event_time > 500) {
                _oskManager.toggleVisible();

                this._last_event_time = event.get_time();
            }
            return Clutter.EVENT_PROPAGATE;
        }
        else
        {
            return this.parent(actor, event);
        }
    },

    _stop_long_press_timer: function() {
        if (this._long_press_timer_id > 0) {
            Mainloop.source_remove(this._long_press_timer_id);
            this._long_press_timer_id = 0;
        }
    },
    _onPreferencesActivate: function(item) {
    },

    destroy: function() {
        this.parent();
    },
});


function init() {
}

let _indicator;

function enable() {
    log('enable');
    _oskManager = new OskManager();

    _indicator = new OnboardIndicator();
    Main.panel.addToStatusArea('onboard-menu', _indicator);

}

function disable() {
    log('disable');
    _oskManager.destroy();
    _oskManager = null;

    _indicator.destroy();
    _indicator = null;
}

