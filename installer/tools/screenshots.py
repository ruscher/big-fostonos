#!/usr/bin/env python3
"""Capturas do BigFoston OS Installer para a documentação (docs/images/instalador/).

Percorre a interface de verdade (GTK 4 + Libadwaita) num display virtual, com um pacote de
versão real e um "cartão" que é uma pasta temporária (nada é gravado em disco nenhum):

  env -u GTK_THEME xvfb-run -a -s "-screen 0 1280x1024x24" \\
      python3 installer/tools/screenshots.py dist/BigFoston-OS-FS707DC-0.6.5.zip docs/images/instalador

SPDX-License-Identifier: GPL-3.0-or-later
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(HERE, "..", "tests"))
os.environ.setdefault("GTK_CSD", "1")

import test_ui as t  # noqa: E402  (FakeStorage, pump, wait_for, find, find_all)
from gi.repository import Adw, Gio, GLib, Graphene, Gtk  # noqa: E402

from bigfoston_installer.core.package import open_package  # noqa: E402
from bigfoston_installer.ui.common import load_css  # noqa: E402
from bigfoston_installer.ui.window import InstallerWindow  # noqa: E402


def save(win, out, name):
    t.pump(0.5)
    w, h = win.get_width(), win.get_height()
    paint = Gtk.WidgetPaintable.new(win)
    snap = Gtk.Snapshot.new()
    paint.snapshot(snap, w, h)
    tex = win.get_renderer().render_texture(snap.to_node(), Graphene.Rect().init(0, 0, w, h))
    tex.save_to_png(os.path.join(out, name + ".png"))
    print(name)


def scroll_to(page, frac):
    sc = t.find(page, lambda x: isinstance(x, Gtk.ScrolledWindow))
    adj = sc.get_vadjustment()
    t.pump(0.3)
    adj.set_value((adj.get_upper() - adj.get_page_size()) * frac)


def main():
    pkg_path, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="bf-shots-")
    sd = os.path.join(tmp, "sd")
    os.makedirs(os.path.join(sd, "iGO"))
    with open(os.path.join(sd, "iGO", "mapa.fbl"), "wb") as f:
        f.write(b"m" * 5000)
    Adw.init()
    load_css()
    app = Adw.Application(application_id="br.org.biglinux.BigFostonInstaller.Shots",
                          flags=Gio.ApplicationFlags.NON_UNIQUE)
    app.register(None)
    st = t.FakeStorage(sd)
    st.cards[0].has_bigfoston = False
    w = InstallerWindow(app, st)
    w.state.backup_dest = os.path.join(tmp, "pc")
    w.set_default_size(900, 720)
    w.present()
    t.pump(0.8)
    top = lambda: w.nav.get_visible_page()  # noqa: E731

    save(w, out, "01-inicio")
    shown = "/home/usuario/BigFoston-Backups"         # o que aparece nas capturas (o real é temporário)
    w.start_install()
    w.state.backup_dest = shown
    t.pump()
    for c in t.find_all(top(), lambda x: isinstance(x, Gtk.CheckButton)):
        c.set_active(True)
    save(w, out, "02-termos")
    top().next_button.emit("clicked")
    t.pump()
    save(w, out, "03-compatibilidade")
    top().next_button.emit("clicked")
    t.pump()
    pkg = open_package(pkg_path)
    pkg.verify()
    w.state.package = pkg
    w.nav.pop()
    t.pump()
    w.push(w.version_page())
    t.pump(0.6)
    save(w, out, "04-versao")
    top().next_button.emit("clicked")
    t.pump()
    save(w, out, "05-metodo")
    top().next_button.emit("clicked")
    t.wait_for(lambda: top().kind == "card" and len(t.find_all(top(), lambda x: isinstance(x, Gtk.CheckButton))) == 1)
    t.find_all(top(), lambda x: isinstance(x, Gtk.CheckButton))[0].set_active(True)
    save(w, out, "06-cartao")
    top().next_button.emit("clicked")
    t.pump()
    save(w, out, "07-backup")
    top().next_button.emit("clicked")
    t.pump()
    save(w, out, "08-resumo")
    top().next_button.emit("clicked")
    t.pump(0.5)
    save(w, out, "09-confirmar")
    dlg = w.get_visible_dialog()
    w.state.backup_dest = os.path.join(tmp, "pc")
    dlg.emit("response", "go")
    dlg.close()
    t.wait_for(lambda: top().kind == "progress")
    # o progresso de verdade passa rápido numa pasta: a captura pega o que estiver na tela
    save(w, out, "10-progresso")
    t.wait_for(lambda: top().kind == "guide", 60)
    for i, frac in enumerate((0.0, 0.45, 0.75, 1.0)):
        scroll_to(top(), frac)
        save(w, out, f"11-guia-{i + 1}")
    w.state.backup_done = os.path.join(shown, os.path.basename(w.state.backup_done))
    top().next_button.emit("clicked")
    t.pump()
    save(w, out, "12-concluido")
    w.nav.pop_to_tag("home")
    w.start_repair()
    t.wait_for(lambda: len(t.find_all(top(), lambda x: isinstance(x, Gtk.CheckButton))) == 1)
    t.find_all(top(), lambda x: isinstance(x, Gtk.CheckButton))[0].set_active(True)
    t.pump(0.1)
    top().next_button.emit("clicked")
    t.wait_for(lambda: t.find(top(), lambda x: isinstance(x, Adw.StatusPage)) is not None)
    save(w, out, "13-verificar")
    w.nav.pop_to_tag("home")
    w.start_restore()
    t.pump()
    save(w, out, "14-restaurar")
    pkg.close()
    GLib.idle_add(w.destroy)
    t.pump(0.2)


if __name__ == "__main__":
    main()
