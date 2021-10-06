import ctypes
import os
import pathlib
import sys

from kivy.core.window import Window
from kivy.lang import Builder
from kivymd.app import MDApp
from kivy.uix.progressbar import ProgressBar
from kivy.metrics import dp
from pygments.lexers.console import PyPyLogLexer

KV = '''
<Home@Screen>
<Components@Screen>
<Additionals@Screen>
<Install@Screen>
#: import os os

ScreenManager:
    Home:
    Components:
    Additionals:
    Install:

<Home>:
    name: "home"
    MDBoxLayout:
        orientation: "vertical"
        padding: "20dp"
        MDLabel:
            text: "Welcome to LCC Setup Wizard"
            font_style: "H4"
            halign: "center"
            font_name: "assets/fonts/AlfaSlabOne-Regular.ttf"
        MDLabel:
            text: "The setup will guide you through the installation of LCC on your machine, it's recommended to [b]close[/b] all other applications before starting the setup."
            halign: "center"
            markup: True
            theme_text_color: "Secondary"
            font_size: "20sp"
        BoxLayout:
    MDRaisedButton:
        text: "NEXT"
        pos_hint: {"center_x": 0.9, "center_y": 0.1}
        on_release: root.manager.current = "components"
    MDFlatButton:
        text: "CLOSE"
        pos_hint: {"center_x": 0.73, "center_y": 0.1}
        on_release: quit()
    MDCard:
        size_hint_y: 0.05
        pos_hint: {"top": 1}
        md_bg_color: 0, 0, 0, 1
        spacing: "0dp"
    MDIconButton:
        theme_text_color: "Custom"
        text_color: 1, 1, 1, 1
        icon: "alpha-x-box"
        user_font_size: "25sp"
        pos_hint: {"center_y": 0.1}
        
<Components>:
    name: "components"
    BoxLayout:
        orientation: 'vertical'
        padding: "20dp"
        MDLabel:
            text: "[b]Select the components you want to install[/b]:"
            halign: "center"
            markup: True
            font_style: "H6"
            font_name: "assets/fonts/AlfaSlabOne-Regular.ttf"
        BoxLayout:
    MDCheckbox:
        pos_hint: {"center_x": 0.25, "center_y": 0.6}
        size_hint: None, None
    MDCheckbox:
        pos_hint: {"center_x": 0.25, "center_y": 0.45}
        size_hint: None, None
    MDLabel:
        text: "LCC (with all standard libraries)"
        halign: "center"
        pos_hint: {"center_y": 0.6}
    MDLabel:
        text: "G++ (Only install if you don't have)"
        halign: "center"
        pos_hint: {"center_y": 0.45}
    MDRaisedButton:
        text: "NEXT"
        pos_hint: {"center_x": 0.9, "center_y": 0.1}
        on_release: root.manager.current = "adds"
    MDFlatButton:
        text: "BACK"
        pos_hint: {"center_x": 0.73, "center_y": 0.1}
        on_release: root.manager.current = "home"
    MDCard:
        size_hint_y: 0.05
        pos_hint: {"top": 1}
        md_bg_color: 0, 0, 0, 1
        

<Additionals>:
    name: "adds"
    MDBoxLayout:
        padding: "20dp"
        spacing: "20dp"
        MDTextField:
            text: f"{app.homePath}{os.sep}LambdaCode{os.sep}"
            pos_hint: {"center_x": 0.4, "center_y": 0.8}
            hint_text: "PATH TO INSTALL LAMBDA-CODE AT"
            id: pathToInstall
        MDIconButton:
            icon: "folder"
            pos_hint: {"center_y": 0.81}
    MDCard:
        size_hint_y: 0.05
        pos_hint: {"top": 1}
        md_bg_color: 0, 0, 0, 1
    MDCheckbox:
        id: createLauncher
        size_hint: None, None
        pos_hint: {"center_x": 0.25, "center_y": 0.6}
    MDLabel:
        text: "Create a 64-bit launcher"
        pos_hint: {"center_y": 0.6}
        halign: "center"
    MDRaisedButton:
        text: "INSTALL"
        pos_hint: {"center_x": 0.9, "center_y": 0.1}
        on_release: app.install()
    MDFlatButton:
        text: "BACK"
        pos_hint: {"center_x": 0.73, "center_y": 0.1}
        on_release: root.manager.current = "components"
        

<Install>:
    name: "install"
    MDBoxLayout:
        orientation: "vertical"
        padding: "20dp"
        spacing: "5dp"
    InstallationProgress:
        allow_stretch: True
        CodeInput:
            readonly: True
'''


class InstallationProgress(ProgressBar):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.size_hint_y: 0.1


def _check_if_admin() -> bool:
    if sys.platform == "win32":
        try:
            return ctypes.windll.shell32.IsUserAnAdmin()
        except Exception:
            return False


class SetupWizard(MDApp):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.homePath = pathlib.Path.home()
        self.path = None

    def build(self):
        Window.size = 550, 400
        Window.borderless = True
        Window.minimum_width, Window.minimum_height = Window.size
        return Builder.load_string(KV)

    def on_start(self):
        Window.left, Window.right = (500, 500)
        return
        if _check_if_admin():
            return
        else:
            ctypes.windll.shell32.ShellExecuteW(
                None, "runas", sys.executable, " ".join(sys.argv), None, 1
            )

    def set_path_env_var(self) -> None:
        os.environ['path'] += self.path

    def install(self) -> None:
        self.root.current = "install"


SetupWizard().run()
