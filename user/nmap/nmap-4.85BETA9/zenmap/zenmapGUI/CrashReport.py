#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (C) 2005 Insecure.Com LLC.
#
# Author: Adriano Monteiro Marques <py.adriano@gmail.com>
#         Frederico Silva Ribeiro <ribeiro.fsilva@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

import sys
import gtk
import gobject
import urllib2
import webbrowser

from zenmapGUI.higwidgets.higdialogs import HIGAlertDialog
from zenmapGUI.higwidgets.higlabels import HIGSectionLabel, HIGHintSectionLabel
from zenmapGUI.higwidgets.higtables import HIGTable
from zenmapGUI.higwidgets.higwindows import HIGWindow
from zenmapGUI.higwidgets.higboxes import HIGHBox, HIGVBox

from zenmapCore.Name import APP_DISPLAY_NAME
from zenmapCore.BugRegister import BugRegister
import zenmapCore.I18N

class CrashReport(HIGWindow):
    def __init__(self, summary, description):
        HIGWindow.__init__(self)
        gtk.Window.__init__(self)
        self.set_title(_('Crash Report'))
        self.set_position(gtk.WIN_POS_CENTER_ALWAYS)
        
        self.response_id = False

        self._create_widgets()
        self._pack_widgets()
        self._connect_widgets()

        self.summary = summary
        self.description = "\n----\n" + description

    def _create_widgets(self):
        self.vbox = HIGVBox()
        self.button_box = gtk.HButtonBox()
        
        self.email_label = gtk.Label(_("""\
An email address is optional. Sometimes we use it to get more information. If \
you provide an email address your report will be marked private so only \
project developers can read it.\
"""))
        self.email_entry = gtk.Entry()

        self.summary_entry = gtk.Entry()

        self.description_label = gtk.Label(_("\
What were you doing when the crash happened?\
"))
        self.description_scrolled = gtk.ScrolledWindow()
        self.description_text = gtk.TextView()

        self.bug_text = gtk.Label(_("""\
An unhandled exception has crashed Zenmap. This dialog allows you to tell us \
what you did to cause the crash and help us to fix it. Submitting the report \
will open a description of the new bug at the bug tracker. Feel free to edit \
the report to remove any identifying information such as your home directory.\
"""))

        self.btn_ok = gtk.Button(stock=gtk.STOCK_OK)
        self.btn_cancel = gtk.Button(stock=gtk.STOCK_CANCEL)

        self.hbox = HIGHBox()
        self.table = HIGTable()

    def _pack_widgets(self):
        self.description_label.set_line_wrap(True)
        self.description_label.set_alignment(0.0, 0.5)
        self.description_scrolled.add(self.description_text)
        self.description_scrolled.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        self.description_scrolled.set_size_request(400, 150)
        self.description_text.set_wrap_mode(gtk.WRAP_WORD)

        self.email_label.set_line_wrap(True)
        self.email_label.set_alignment(0.0, 0.5)

        self.bug_text.set_line_wrap(True)

        self.hbox.set_border_width(12)
        self.vbox.set_border_width(6)
        
        self.table.attach_label(gtk.Label(_("Summary")), 0, 1, 0, 1)
        self.table.attach_entry(self.summary_entry, 1, 2, 0, 1)

        self.table.attach_entry(self.description_label, 0, 2, 1, 2)
        self.table.attach_entry(self.description_scrolled, 0, 2, 2, 3)
        
        self.table.attach(self.email_label, 0, 2, 3, 4)
        self.table.attach_label(gtk.Label(_("Email")), 0, 1, 4, 5)
        self.table.attach_entry(self.email_entry, 1, 2, 4, 5)

        self.hbox._pack_expand_fill(self.bug_text)

        self.button_box.set_layout(gtk.BUTTONBOX_END)
        self.button_box.pack_start(self.btn_ok)
        self.button_box.pack_start(self.btn_cancel)
        
        self.vbox._pack_noexpand_nofill(self.hbox)
        self.vbox._pack_expand_fill(self.table)
        self.vbox._pack_noexpand_nofill(self.button_box)
        self.add(self.vbox)

    def _connect_widgets(self):
        self.btn_ok.connect("clicked", self.send_report)
        self.btn_cancel.connect("clicked", self.close)
        self.connect("delete-event", self.close)

    def send_report(self, widget):
        bug_register = BugRegister()
        description_text = self.description
        private = False
        if self.email != "":
            description_text += "\n\n" + self.email
            private = True
        report_url = bug_register.get_report_url(self.summary,
            description_text, private = private)

        # First we try reporting the bug with a web browser because that gives
        # better feedback on what happened. If that doesn't work try with
        # urllib2.
        try:
            webbrowser.open(report_url)
        except webbrowser.Error:
            try:
                urllib2.urlopen(report_url)
                ok_dialog = HIGAlertDialog(type=gtk.MESSAGE_INFO,
                    message_format=_("Bug reported"),
                    secondary_text=_("The bug was successfully reported."))
                ok_dialog.run()
                ok_dialog.destroy()
            except urllib2.URLError, e:
                cancel_dialog = HIGAlertDialog(type=gtk.MESSAGE_ERROR,
                    message_format=_("Bug not reported"),
                    secondary_text=_("This error occurred while trying to report the bug:\n\n" + str(e)))
                cancel_dialog.run()
                cancel_dialog.destroy()

        self.close()

    def close(self, widget=None, event=None):
        self.destroy()
        gtk.main_quit()
        sys.exit(0)

    def get_summary(self):
        return self.summary_entry.get_text()

    def set_summary(self, summary):
        summary = summary.decode("UTF-8", "replace")
        self.summary_entry.set_text(summary)

    def get_description(self):
        buff = self.description_text.get_buffer()
        return buff.get_text(buff.get_start_iter(), buff.get_end_iter())

    def set_description(self, description):
        description = description.decode("UTF-8", "replace")
        self.description_text.get_buffer().set_text(description)

    def get_email(self):
        return self.email_entry.get_text()

    def set_email(self, email):
        self.email_entry.set_text(email)

    def run_unblocked(self):
        if not self.modal:
            self.set_modal(True)
        self.show_all()

    summary = property(get_summary, set_summary)
    description = property(get_description, set_description)
    email = property(get_email, set_email)


if __name__ == "__main__":
    c = CrashReport("Sumariu", "Descricao")
    c.show_all()
    c.connect("delete-event", lambda x, y: gtk.main_quit())
    
    gtk.main()
