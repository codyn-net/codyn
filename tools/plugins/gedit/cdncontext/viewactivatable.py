# -*- coding: utf-8 -*-
#
#  viewactivatable.py - cdncontext gedit plugin
#
#  Copyright (C) 2011 - Jesse van den Kieboom
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330,
#  Boston, MA 02111-1307, USA.

from shareddata import SharedData
import parser

from gi.repository import GObject, Gedit, Pango, Gtk

class ViewActivatable(GObject.Object, Gedit.ViewActivatable):
    view = GObject.property(type=Gedit.View)

    def __init__(self):
        GObject.Object.__init__(self)

        self.timeout = None
        self.cursor_timeout = None
        self.parser = None
        self.iscdn = False
        self.last_parsed = []
        self.active_handlers = []
        self.handlers = []
        self.last_error = None

    def do_activate(self):
        doc = self.view.get_buffer()

        self.handlers = [
            doc.connect('notify::language', self.on_language_changed),
            doc.connect_after('insert-text', self.on_insert_text)
        ]

        self.error_tag = doc.create_tag('CdnParsed::error')
        doc.get_tag_table().remove(self.error_tag)
        self.error_tag.props.underline = Pango.Underline.ERROR

        self.language_changed()

    def special_char(self, c):
        return c.isalnum() or c == '_'

    def on_insert_text(self, doc, iter, text, length):
        if not self.iscdn or length == 0:
            return

        start = iter.copy()
        end = iter.copy()

        while not end.is_end() and self.special_char(end.get_char()):
            end.forward_char()

        start.backward_char()

        while not start.is_start() and self.special_char(start.get_char()):
            start.backward_char()

        if start.get_text(end) == '^2':
            doc.begin_user_action()
            doc.delete(start, end);
            doc.insert(start, '²')
            iter.assign(start)
            doc.end_user_action()
        elif start.get_char() == '\\':
            start.forward_char()
            t = start.get_text(end)

            smap = {
                'sqrt': '√',
                'sum': '∑',
                'cdot': '∙',
                'prod': '∏',
                'transpose': 'ᵀ',
                'alpha': 'α',
                'beta': 'β',
                'gamma': 'γ',
                'delta': 'δ',
                'epsilon': 'ε',
                'zeta': 'ζ',
                'eta': 'η',
                'theta': 'θ',
                'iota': 'ι',
                'kappa': 'κ',
                'lamda': 'λ',
                'mu': 'μ',
                'nu': 'ν',
                'xi': 'ξ',
                'omicron': 'ο',
                'pi': 'π',
                'rho': 'ρ',
                'fsigma': 'ς',
                'sigma': 'σ',
                'tau': 'τ',
                'upsilon': 'υ',
                'phi' :'φ',
                'chi' :'χ',
                'psi' :'ψ',
                'omega' :'ω',
                'Alpha': 'Α',
                'Beta': 'Β',
                'Gamma': 'Γ',
                'Delta': 'Δ',
                'Epsilon': 'Ε',
                'Zeta': 'Ζ',
                'Eta': 'Η',
                'Theta': 'Θ',
                'Iota': 'Ι',
                'Kappa': 'Κ',
                'Lamda': 'Λ',
                'Mu': 'Μ',
                'Nu': 'Ν',
                'Xi': 'Ξ',
                'Omicron': 'Ο',
                'Pi': 'Π',
                'Rho': 'Ρ',
                'Sigma': 'Σ',
                'Tau': 'Τ',
                'Upsilon': 'Υ',
                'Phi': 'Φ',
                'Chi': 'Χ',
                'Psi': 'Ψ',
                'Omega': 'Ω',
                'partial': '∂',
                'inf': '∞'
            }

            if t in smap:
                repl = smap[t]
                start.backward_char()

                doc.begin_user_action()
                doc.delete(start, end)
                doc.insert(start, repl)
                iter.assign(start)
                doc.end_user_action()
                return

            diacmap = {
                'hat': '\xcc\x82',
                'tilde': '\xcc\x83',
                'bar': '\xcc\x84',
                'dot': '\xcc\x87',
                'ddot': '\xcc\x88',
            }

            if t in diacmap:
                start.backward_chars(2)
                repl = start.get_char() + diacmap[t]

                doc.begin_user_action()
                doc.delete(start, end)
                doc.insert(start, repl)
                iter.assign(start)
                doc.end_user_action()

    def on_query_tooltip(self, view, x, y, keyboard_mode, tooltip):
        if not self.last_error:
            return False

        xy = self.view.window_to_buffer_coords(Gtk.TextWindowType.WIDGET, x, y)

        if not xy:
            return False

        piter = self.view.get_iter_at_location(xy[0], xy[1])

        if not piter:
            return False

        if not piter.has_tag(self.error_tag):
            return False

        tooltip.set_text(self.last_error.message)
        return True

    def cancel(self):
        doc = self.view.get_buffer()

        if not self.iscdn:
            return

        doc.get_tag_table().remove(self.error_tag)

        for x in self.active_handlers:
            x[0].disconnect(x[1])

        self.active_handlers = []

        if self.parser:
            self.parser.cancel()
            self.parser = None

        if self.timeout is not None:
            GObject.source_remove(self.timeout)
            self.timeout = None

    def language_changed(self):
        doc = self.view.get_buffer()
        lang = doc.get_language()

        iscdn = (lang and lang.get_id() == 'cdn')

        if iscdn == self.iscdn:
            return

        self.cancel()
        self.iscdn = iscdn

        if self.iscdn:
            self.active_handlers = [
                [doc, doc.connect('changed', self.on_doc_changed)],
                [doc, doc.connect('saved', self.on_doc_saved)],
                [doc, doc.connect('cursor-moved', self.on_cursor_moved)],
                [self.view, self.view.connect('query-tooltip', self.on_query_tooltip)]
            ]

            doc.get_tag_table().add(self.error_tag)

            self.request_update()

    def on_language_changed(self, doc, spec):
        self.language_changed()

    def on_doc_saved(self, doc, error):
        self.request_update()

    def on_doc_changed(self, doc):
        self.request_update()

    def clear_parsed(self):
        doc = self.view.get_buffer()
        bounds = doc.get_bounds()

        doc.remove_tag(self.error_tag, bounds[0], bounds[1])

    def update_parsed_error(self, data):
        self.last_parsed = []
        self.last_error = data

        message = data.message
        lstart = data.line_start
        lend = data.line_end
        cstart = data.column_start
        cend = data.column_end

        doc = self.view.get_buffer()
        piter = doc.get_iter_at_line(max(0, lstart - 1))

        if piter:
            pend = piter.copy()

            piter.forward_chars(max(cstart - 1, 0))

            if lend > lstart:
                pend.forward_lines(lend - lstart)

            pend.forward_chars(max(cend, 0))

            doc.apply_tag(self.error_tag, piter, pend)

        SharedData().panel.model.update(None)

    def on_cursor_moved(self, doc):
        self.request_update()

    def update_parsed_ok(self, data):
        self.last_parsed = data
        self.last_error = None

        if len(data) > 0:
            SharedData().panel.model.update(data[0])
        else:
            SharedData().panel.model.update(None)

        SharedData().panel.treeview.expand_all()

    def update_parsed(self, ret):
        self.clear_parsed()

        if not ret:
            return

        if ret['status'] == 'error':
            self.update_parsed_error(ret['data'])
        else:
            self.update_parsed_ok(ret['data'])

    def on_parser_finished(self, ret):
        self.parser = None
        self.update_parsed(ret)

    def run_parser_at_cursor(self):
        if self.parser != None:
            self.parser.cancel()
            self.parser = None

        buf = self.view.get_buffer()

        self.parser = parser.Parser(buf, self.on_parser_finished)

        if SharedData().panel.get_mapped():
            ins = buf.get_iter_at_mark(buf.get_insert())
            l = ins.get_line() + 1
            c = ins.get_line_offset() + 1
        else:
            l = -1
            c = -1

        self.parser.run(l, c)

    def on_request_timeout(self):
        self.timeout = None
        self.run_parser_at_cursor()
        return False

    def request_update(self):
        if not self.parser is None:
            self.parser.cancel()
            self.parser = None

        if not self.timeout is None:
            GObject.source_remove(self.timeout)
            self.timeout = None

        self.timeout = GObject.timeout_add(300, self.on_request_timeout)

    def do_deactivate(self):
        doc = self.view.get_buffer()

        for handler in self.handlers:
            doc.disconnect(handler)

        self.handlers = []

        self.cancel()

        self.view = None

# vi:ex:ts=4:et
