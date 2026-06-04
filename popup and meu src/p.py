import os, sys, subprocess, psutil, keyboard,requests, discord,  tkinter as tk
import tkinter.font as tkfont
import win32api as wapi, win32security as wsec, win32con as wcon, win32job as wjob
import ctypes, ctypes.wintypes as wt, winreg, re; import pygetwindow as gw; 
from datetime import datetime, timedelta; from random import choice
from time import perf_counter, time, sleep; import threading
from random import choices
# pip install pywin32 keyboard pygetwindow discord

# Code
GY, Y, P, B, G, W, R, C = '\033[90m', '\033[1;33m', '\033[1;95m', '\033[1;34m', '\033[1;32m', '\033[1;97m', '\033[1;31m', '\033[1;36m'

k32, nt = ctypes.windll.kernel32, ctypes.windll.ntdll; u32 = ctypes.windll.user32

def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    except Exception:
        return False

if not is_admin():
    script = os.path.abspath(sys.argv[0])
    params = subprocess.list2cmdline([script] + sys.argv[1:])
    print(f"{Y}[*] run as admin.{W}")
    result = ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, params, None, 1)
    if int(result) <= 32:
        print(f"{R}[!] Failed to elevate to admin. Please run this script as Administrator.{W}")
        sys.exit(1)
    sys.exit(0)

k32.SetConsoleTitleW(bytes(''.join(choices('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789', k=128)), 'utf-8')); os.system('cls')

hwnd = k32.GetConsoleWindow(); vgc_path = "C:\\Program Files\\Riot Vanguard\\vgc.exe"
if os.path.exists(vgc_path): ctypes.windll.user32.SendMessageW(hwnd, 0x80, 0, ctypes.windll.shell32.ExtractIconW(0, vgc_path, 0))
style = u32.GetWindowLongW(hwnd, -16)
u32.EnableMenuItem(u32.GetSystemMenu(hwnd, False), 0xF060, 1)
u32.SetWindowLongW(hwnd, -16, (style | 0x80000 | 0x20000) & ~0x40000 & ~0x10000)
u32.SetWindowLongW(hwnd, -20, u32.GetWindowLongW(hwnd, -20) | 0x80000)
u32.SetLayeredWindowAttributes(hwnd, 0, 230, 2)
u32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x27)
u32.ShowWindow(hwnd, 0)
mutex = k32.CreateMutexW(None, False, b"PopUp  | Imperium.sys")
if k32.GetLastError() == 183: input("\n" + f"{R}  [!] Already Running !{Y} Close Old Instance."); sys.exit(0)

def SetDebugPrev(): # Loop Force SeDebugPrivilege
    while True:
        try:
            for p in [os.getpid(), os.getppid(), wapi.GetCurrentProcess()]:
                tok = wsec.OpenProcessToken(p, wcon.TOKEN_ADJUST_PRIVILEGES | wcon.TOKEN_QUERY)
                priv = wsec.LookupPrivilegeValue(None, wcon.SE_DEBUG_NAME)
                wsec.AdjustTokenPrivileges(tok, False, [(priv, wcon.SE_PRIVILEGE_ENABLED)])
        except: pass
        sleep(0.5)
threading.Thread(target=SetDebugPrev, daemon=True).start()

def run(cmd): subprocess.run(cmd, shell=True, stdout=-1, stderr=-1)
def cls(): x = '' # os.system('cls')

def kill_process_by_name(name):
    for proc in psutil.process_iter(['name', 'pid']):
        try:
            if proc.info['name'] and proc.info['name'].lower() == name.lower():
                proc.kill()
        except Exception:
            pass


RiotLauncherProc = None

RiotLauncherNames = [
    'RiotClientServices.exe',
    'RiotClient.exe',
    'RiotClientUx.exe',
    'RiotClientServicesHelper.exe'
]


def cleanup():
    global RiotLauncherProc
    set_overlay_text('[!] Closing bypass...\nCleaning up changes...')
    if dns_suspended:
        BoolDnsCache(False)
    run('sc stop vgc')
    if RiotLauncherProc:
        try:
            RiotLauncherProc.terminate()
        except Exception:
            pass
    kill_process_by_name('VALORANT-Win64-Shipping.exe')
    kill_process_by_name('vgc.exe')
    for name in RiotLauncherNames:
        kill_process_by_name(name)
    set_overlay_text('[+] Cleanup complete. Exiting...')
    sleep(1)


def SafeExit(_=None):
    set_overlay_text('[!] Safe Exit ...')
    cleanup()
    run('w32tm /resync')
    if overlay_window and hasattr(overlay_window, 'quit'):
        try:
            overlay_window.quit()
        except Exception:
            pass
    os._exit(1); os.kill(os.getpid(), 1)
wapi.SetConsoleCtrlHandler(lambda signal_type: SafeExit(), True)
keyboard.add_hotkey('f10', SafeExit, suppress=False)
keyboard.add_hotkey('f9', lambda: toggle_overlay(), suppress=False)

overlay_visible = True
overlay_window = None
overlay_title = 'Imperium.sys - Popup Bypass'
overlay_text = '[*] Waiting Valorant ...'
dns_suspended = False


def set_overlay_text(text):
    global overlay_text
    overlay_text = text


def toggle_overlay():
    global overlay_visible
    overlay_visible = not overlay_visible


def overlay_loop():
    global overlay_window, overlay_visible, overlay_text

    root = tk.Tk()
    root.title('Bypass Overlay')
    root.geometry('420x180+10+10')
    root.overrideredirect(True)
    root.attributes('-topmost', True)
    root.attributes('-alpha', 0.82)
    root.update_idletasks()
    overlay_hwnd = ctypes.windll.user32.GetParent(root.winfo_id())
    ex_style = u32.GetWindowLongW(overlay_hwnd, -20)
    u32.SetWindowLongW(overlay_hwnd, -20, ex_style | 0x20 | 0x80000)
    root.configure(bg='black')

    body_font = tkfont.Font(family='Times New Roman', size=12, weight='bold')
    title_font = tkfont.Font(family='Times New Roman', size=18, weight='bold')

    canvas = tk.Canvas(root, bg='black', highlightthickness=0)
    canvas.pack(fill='both', expand=True)
    marker = canvas.create_line(4, 4, 44, 4, fill='#ff4c4c', width=3)

    overlay_textbox = tk.Text(root, bg='black', fg='white', bd=0, highlightthickness=0,
                               font=body_font, wrap='word')
    overlay_textbox.tag_configure('title', foreground='#ff4c4c', font=title_font)
    overlay_textbox.tag_configure('good', foreground='#7CFC00', font=body_font)
    overlay_textbox.tag_configure('bad', foreground='#FF4040', font=body_font)
    overlay_textbox.tag_configure('neutral', foreground='#00BFFF', font=body_font)
    overlay_textbox.tag_configure('info', foreground='#00FFAA', font=body_font)
    overlay_textbox.tag_configure('default', foreground='white', font=body_font)
    overlay_textbox.place(x=10, y=10, width=400, height=160)
    overlay_textbox.configure(state='disabled')

    marker_pos = 0
    perimeter = (416 - 4) * 2 + (176 - 4) * 2
    def animate_border():
        nonlocal marker_pos
        marker_pos = (marker_pos + 4) % perimeter
        segment = 40
        if marker_pos < 412:
            x_start = 4 + marker_pos
            x_end = min(x_start + segment, 416)
            coords = (x_start, 4, x_end, 4)
        elif marker_pos < 412 + 172:
            y_start = 4 + (marker_pos - 412)
            y_end = min(y_start + segment, 176)
            coords = (416, y_start, 416, y_end)
        elif marker_pos < 412 + 172 + 412:
            x_start = 416 - (marker_pos - 584)
            x_end = max(x_start - segment, 4)
            coords = (x_start, 176, x_end, 176)
        else:
            y_start = 176 - (marker_pos - 996)
            y_end = max(y_start - segment, 4)
            coords = (4, y_start, 4, y_end)
        canvas.coords(marker, *coords)
        root.after(50, animate_border)
    animate_border()

    def format_line(line):
        tag = 'default'
        stripped = line.strip()
        if stripped.startswith('Imperium.sys') or 'Popup Bypass' in stripped:
            tag = 'title'
        elif stripped.startswith('[+]') or 'active' in stripped.lower() or 'complete' in stripped.lower() or 'success' in stripped.lower():
            tag = 'good'
        elif stripped.startswith('[!]') or 'error' in stripped.lower() or 'fail' in stripped.lower() or 'cannot' in stripped.lower() or 'closing' in stripped.lower():
            tag = 'bad'
        elif stripped.startswith('[*]') or 'waiting' in stripped.lower() or 'initializing' in stripped.lower() or 'debug' in stripped.lower():
            tag = 'neutral'
        return tag

    last_text = None

    def refresh():
        nonlocal root, last_text
        if overlay_text != last_text:
            overlay_textbox.configure(state='normal')
            overlay_textbox.delete('1.0', 'end')
            overlay_textbox.insert('end', overlay_title + '\n\n', 'title')
            for line in overlay_text.splitlines():
                tag = format_line(line)
                overlay_textbox.insert('end', line + '\n', tag)
            overlay_textbox.configure(state='disabled')
            last_text = overlay_text
        if overlay_visible:
            if root.state() == 'withdrawn':
                root.deiconify()
        else:
            if root.state() != 'withdrawn':
                root.withdraw()
        root.after(100, refresh)

    root.after(100, refresh)
    overlay_window = root
    root.mainloop()


threading.Thread(target=overlay_loop, daemon=True).start()

RiotServicePath = None
try: # Find Riot Client / Service Path
    with winreg.OpenKey(winreg.HKEY_CLASSES_ROOT, "riotclient\\DefaultIcon") as key:
        match = re.search(r'^(.*\.exe)', winreg.QueryValueEx(key, "")[0])
        dq = '"'
        RiotServicePath = f"{match.group(1).replace(dq, '')}"
        RiotClientPath = os.path.dirname(f'{RiotServicePath}\\RiotClientElectron\\Riot Client.exe')
except: pass
if RiotServicePath is None: input(f"{R}[!] Riot Client Not Found!"); SafeExit()

def BoolDnsCache(bool):
    global dns_suspended
    fdc = False 
    for proc in psutil.process_iter(['name', 'pid', 'cmdline']):
        cmdline = proc.info['cmdline'] or []
        if proc.info['name'] == "svchost.exe":
            if any('dnscache' in str(arg).lower() for arg in cmdline):
                try:
                    if bool:
                        proc.suspend()
                        dns_suspended = True
                    else:
                        proc.resume()
                        dns_suspended = False
                    fdc = True 
                except Exception:
                    pass
    if not fdc:
        set_overlay_text('Cannot bypass popup.\nEnsure admin and Valorant are running.')
    return fdc

def InitVgc():
    run(f'sc stop vgc')
    [run(cmd) for cmd in ['sc stop wm32time','sc start wm32time','w32tm /resync']]
    run('del /f /s /q "C:\\Program Files\\Riot Vanguard\\Logs\\*"')
    run('del /f /s /q "C:\\Windows\\392667600.dat"')
    run('del /f /s /q "C:\\Windows\\Prefetch\\*"')
    run('ipconfig /flushdns')
    run(f'sc config vgc start= auto & sc start vgc')
    
def main():
    try:
        set_overlay_text('Initializing bypass...\nStarting Vanguard service...')
        InitVgc()

        global RiotLauncherProc
        RiotLauncherProc = subprocess.Popen(f'"{RiotServicePath}"', creationflags=0x08000000)
        last_v = None; pub = None; utb = None;vgcm = []; vgcm.clear()
        cheat_inj = None; vgbp = None 
        while True:
            vgc = next((p for p in psutil.process_iter(['name','pid']) if p.info['name'] == "vgc.exe"), None)
            valo = any("VALORANT  " in w.title for w in gw.getAllWindows() if w.title)
            if valo != last_v:
                last_v = valo
                if valo: 
                    set_overlay_text('[*] Valorant found.\n[*] Waiting for VGC...')
                else:
                    set_overlay_text('[*] Waiting Valorant ...')
                    BoolDnsCache(False); pub = False; InitVgc()
            if valo and vgc and not utb:
                if pub is None or not pub:
                    set_overlay_text('[*] Bypassing popup...\n[*] Waiting for VGC threads...')
                    while vgc.num_threads() <= 15: pass
                    sleep(0.035); BoolDnsCache(True); pub = True
                    set_overlay_text('[+] Popup bypass active.\nF9 hide/show.\nF10 close.')
            sleep(7.5) if vgbp else sleep(3)
    except Exception as e:
        set_overlay_text(f'Error: {e}')
main()