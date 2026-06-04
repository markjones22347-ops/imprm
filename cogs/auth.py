"""
Imperium Bot — Auth / Key Management cog

Admin commands (Founder role only):
  /genkey          — generate a key via modal
  /disablekey      — disable a key
  /deletekey       — delete a key
  /bulkdeletekeys  — delete multiple keys at once
  /managekey       — view/edit a single key
  /managekeys      — view/edit multiple keys
  /showallkeys     — list every key with full details
  /hwidreset       — reset HWID binding on one or more keys
  /unclaim         — unclaim a key (wipes username/password/hwid, keeps key)

Customer commands:
  /register        — claim a key and create login credentials (open to all)
  /download        — get the download link (Customer role required)
"""

import discord
from discord import app_commands, ui
from discord.ext import commands
import random
import string
import os
from datetime import datetime, timezone

from cogs.database import (
    create_key, get_key, get_all_keys,
    disable_key, enable_key, delete_key, delete_keys,
    update_key, register_key, reset_hwid, key_exists,
    get_download_url, set_download_url,
)

# ─── Role IDs ─────────────────────────────────────────────────────────────────
FOUNDER_ROLE_ID  = 1511851579446137003
CUSTOMER_ROLE_ID = 1511852608858492938

# IMPERIUM-XXXX-XXXX-XXXX = 8 + 1 + 4 + 1 + 4 + 1 + 4 = 23 chars
KEY_PREFIX = "IMPERIUM"
KEY_LENGTH = 23


# ─── Helpers ──────────────────────────────────────────────────────────────────

def _gen_key() -> str:
    chars = string.ascii_uppercase + string.digits
    segments = ["".join(random.choices(chars, k=4)) for _ in range(3)]
    return f"{KEY_PREFIX}-" + "-".join(segments)


def _is_founder(member: discord.Member) -> bool:
    return any(r.id == FOUNDER_ROLE_ID for r in member.roles)


def _is_customer(member: discord.Member) -> bool:
    return any(r.id == CUSTOMER_ROLE_ID for r in member.roles)


def _fmt_ts(iso: str | None) -> str:
    if not iso:
        return "—"
    try:
        dt = datetime.fromisoformat(iso)
        return f"<t:{int(dt.timestamp())}:F>"
    except Exception:
        return iso


def _validate_key_format(key: str) -> bool:
    """IMPERIUM-XXXX-XXXX-XXXX — exactly 23 chars, correct structure."""
    parts = key.split("-")
    if len(parts) != 4:
        return False
    if parts[0] != KEY_PREFIX:
        return False
    if not all(len(p) == 4 for p in parts[1:]):
        return False
    return True


# ══════════════════════════════════════════════════════════════════════════════
#  Modals
# ══════════════════════════════════════════════════════════════════════════════

class SetDownloadModal(ui.Modal, title="Set Download Link"):
    url_input = ui.TextInput(
        label="Download URL",
        placeholder="https://example.com/imperium.rar",
        style=discord.TextStyle.short,
        max_length=500,
    )

    async def on_submit(self, interaction: discord.Interaction):
        url = self.url_input.value.strip()
        if not url.startswith("http"):
            await interaction.response.send_message("❌ Invalid URL.", ephemeral=True)
            return
        await set_download_url(url)
        await interaction.response.send_message(
            f"✅ Download link updated.\n`{url}`", ephemeral=True
        )


class GenKeyModal(ui.Modal, title="Generate Key"):
    custom_key = ui.TextInput(
        label="Custom key (optional)",
        placeholder=f"{KEY_PREFIX}-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=KEY_LENGTH,
        required=False,
    )
    duration = ui.TextInput(
        label="Duration",
        placeholder="e.g. 30d, 7d, lifetime",
        style=discord.TextStyle.short,
        max_length=50,
    )
    note = ui.TextInput(
        label="Note (optional)",
        placeholder="e.g. for @username, promo, etc.",
        style=discord.TextStyle.short,
        max_length=200,
        required=False,
    )
    quantity = ui.TextInput(
        label="Quantity (1–25)",
        placeholder="1",
        style=discord.TextStyle.short,
        max_length=2,
        default="1",
    )

    async def on_submit(self, interaction: discord.Interaction):
        custom = self.custom_key.value.strip().upper()
        try:
            qty = max(1, min(25, int(self.quantity.value.strip())))
        except ValueError:
            qty = 1

        generated = []
        if custom:
            if qty != 1:
                await interaction.response.send_message(
                    "❌ Custom key generation supports only a single key at a time.",
                    ephemeral=True,
                )
                return
            if not _validate_key_format(custom):
                await interaction.response.send_message(
                    f"❌ Invalid key format. Keys must look like `{KEY_PREFIX}-XXXX-XXXX-XXXX`.",
                    ephemeral=True,
                )
                return
            if key_exists(custom):
                await interaction.response.send_message(
                    f"❌ Key `{custom}` already exists.", ephemeral=True,
                )
                return

            create_key(custom, self.duration.value.strip(), interaction.user.id)
            generated.append(custom)
        else:
            for _ in range(qty):
                key = _gen_key()
                while key_exists(key):
                    key = _gen_key()
                create_key(key, self.duration.value.strip(), interaction.user.id)
                generated.append(key)

        note_line  = f"\n**Note:** {self.note.value.strip()}" if self.note.value.strip() else ""
        keys_block = "\n".join(generated)

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay(f"## {'Key' if len(generated) == 1 else f'{len(generated)} Keys'} Generated"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                f"**Duration:** {self.duration.value.strip()}{note_line}\n"
                f"**Generated by:** {interaction.user.mention}\n\n"
                f"```\n{keys_block}\n```"
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class RegisterModal(ui.Modal, title="Register — Claim Your Key"):
    key_input = ui.TextInput(
        label="Your Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=KEY_LENGTH,   # exactly 23
    )
    username_input = ui.TextInput(
        label="Username",
        placeholder="Choose a username for the loader",
        style=discord.TextStyle.short,
        max_length=32,
    )
    password_input = ui.TextInput(
        label="Password",
        placeholder="Choose a password for the loader",
        style=discord.TextStyle.short,
        max_length=64,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key      = self.key_input.value.strip().upper()
        username = self.username_input.value.strip()
        password = self.password_input.value.strip()

        # Validate format before hitting the DB
        if not _validate_key_format(key):
            await interaction.response.send_message(
                f"❌ Invalid key format. Keys must look like `{KEY_PREFIX}-XXXX-XXXX-XXXX`.",
                ephemeral=True,
            )
            return

        if not username or not password:
            await interaction.response.send_message(
                "❌ Username and password cannot be empty.", ephemeral=True
            )
            return

        success, err = register_key(key, username, password, interaction.user.id)

        if not success:
            await interaction.response.send_message(
                f"❌ Registration failed: **{err}**", ephemeral=True
            )
            return

        # Assign Customer role on success
        customer_role = interaction.guild.get_role(CUSTOMER_ROLE_ID)
        if customer_role and customer_role not in interaction.user.roles:
            try:
                await interaction.user.add_roles(customer_role, reason="Registered an Imperium key")
            except discord.Forbidden:
                pass

        # Defer so we can send two follow-up messages
        await interaction.response.defer(ephemeral=True, thinking=False)

        # ── Success message ───────────────────────────────────────────────────
        reg_view = ui.LayoutView()
        reg_view.add_item(ui.Container(
            ui.TextDisplay("## ✅ Registration Successful"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                f"Your key has been claimed. Here are your login credentials:\n\n"
                f"**Username:** `{username}`\n"
                f"**Password:** the one you just set\n"
                f"**Key:** `{key}`\n\n"
                f"-# Save these — you cannot recover your password."
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium — Registration"),
        ))
        await interaction.response.send_message(view=reg_view, ephemeral=True)

        # ── Getting started guide (second message) ────────────────────────────
        guide_view = ui.LayoutView()
        guide_view.add_item(ui.Container(
            ui.TextDisplay("## 📖 Getting Started with Imperium"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                "**Step 1 — Download**\n"
                "Use `/download` to get the latest `Loader.exe`.\n\n"
                "**Step 2 — Run the Loader**\n"
                "Open `Loader.exe`. A console window will appear.\n"
                "Enter your **username** and **password** when prompted.\n\n"
                "**Step 3 — Launch Roblox**\n"
                "Make sure Roblox (`RobloxPlayerBeta.exe`) is already running before or after you log in — the loader will find it automatically.\n\n"
                "**Step 4 — Open the Menu**\n"
                "Press **INSERT** to open/close the Imperium menu overlay.\n"
                "The menu appears on top of your Roblox window.\n\n"
                "**Step 5 — Using Features**\n"
                "• **Aimbot / Silent Aim** — enable and set a keybind to activate\n"
                "• **Visuals (ESP)** — toggle boxes, names, health bars under the Visuals tab\n"
                "• **Rage** — hitbox expander, rapidfire, hitsounds, etc.\n"
                "• **Movement** — speedhack and flyhack with keybind support\n"
                "• **Settings** — change theme color, save/load configs, toggle watermark\n\n"
                "**HWID Binding**\n"
                "Your hardware ID binds automatically on first launch. If you change PC, ask an admin to use `/hwidreset` with your key.\n\n"
                "**Need help?** Open a support ticket with `/ticket` or ask in the support channel."
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium — Getting Started Guide"),
        ))
        await interaction.followup.send(view=guide_view, ephemeral=True)


class DisableKeyModal(ui.Modal, title="Disable Key"):
    key_input = ui.TextInput(
        label="Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=KEY_LENGTH,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        if disable_key(key):
            await interaction.response.send_message(f"🔴 Key `{key}` has been **disabled**.", ephemeral=True)
        else:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)


class DeleteKeyModal(ui.Modal, title="Delete Key"):
    key_input = ui.TextInput(
        label="Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=KEY_LENGTH,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        if delete_key(key):
            await interaction.response.send_message(f"🗑️ Key `{key}` has been **deleted**.", ephemeral=True)
        else:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)


class BulkDeleteModal(ui.Modal, title="Bulk Delete Keys"):
    keys_input = ui.TextInput(
        label="Keys (one per line)",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX\nIMPERIUM-YYYY-YYYY-YYYY",
        style=discord.TextStyle.paragraph,
        max_length=2000,
    )

    async def on_submit(self, interaction: discord.Interaction):
        raw            = self.keys_input.value.strip().splitlines()
        keys           = [k.strip().upper() for k in raw if k.strip()]
        deleted, not_found = delete_keys(keys)

        lines = []
        if deleted:
            lines.append(f"**Deleted ({len(deleted)}):**\n```\n" + "\n".join(deleted) + "\n```")
        if not_found:
            lines.append(f"**Not found ({len(not_found)}):**\n```\n" + "\n".join(not_found) + "\n```")

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay("## Bulk Delete Result"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("\n\n".join(lines) or "Nothing to delete."),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class ManageKeyModal(ui.Modal, title="Manage Key"):
    key_input = ui.TextInput(
        label="Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=KEY_LENGTH,
    )
    new_duration = ui.TextInput(
        label="New Duration (leave blank to keep)",
        placeholder="e.g. lifetime, 30d",
        style=discord.TextStyle.short,
        max_length=50,
        required=False,
    )
    toggle_disabled = ui.TextInput(
        label="Enable / Disable (enable | disable | leave blank)",
        placeholder="enable  or  disable",
        style=discord.TextStyle.short,
        max_length=10,
        required=False,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        rec = get_key(key)
        if not rec:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)
            return

        changes = []
        kwargs  = {}

        if self.new_duration.value.strip():
            kwargs["duration"] = self.new_duration.value.strip()
            changes.append(f"Duration → `{self.new_duration.value.strip()}`")

        toggle = self.toggle_disabled.value.strip().lower()
        if toggle == "disable":
            kwargs["disabled"] = True
            changes.append("Status → 🔴 Disabled")
        elif toggle == "enable":
            kwargs["disabled"] = False
            changes.append("Status → 🟢 Active")

        if kwargs:
            update_key(key, **kwargs)

        rec         = get_key(key)
        change_text = "\n".join(changes) if changes else "No changes made."

        gen_user     = "—"
        claimed_user = "—"
        if rec.get("generated_by"):
            m = interaction.guild.get_member(rec["generated_by"])
            gen_user = str(m) if m else f"ID:{rec['generated_by']}"
        if rec.get("claimed_by_discord"):
            m = interaction.guild.get_member(rec["claimed_by_discord"])
            claimed_user = str(m) if m else f"ID:{rec['claimed_by_discord']}"

        summary = (
            f"**Key:** `{key}`\n"
            f"**Duration:** {rec.get('duration','—')}\n"
            f"**Status:** {'🔴 Disabled' if rec.get('disabled') else '🟢 Active'}\n"
            f"**Generated by:** {gen_user}\n"
            f"**Claimed by:** {claimed_user}\n"
            f"**Username:** `{rec.get('username') or '—'}`\n"
            f"**HWID:** `{rec.get('hwid') or 'unbound'}`"
        )

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay(f"## Manage Key — `{key}`"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(f"**Changes applied:**\n{change_text}"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(summary),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class ManageKeysModal(ui.Modal, title="Manage Multiple Keys"):
    keys_input = ui.TextInput(
        label="Keys (one per line)",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX\nIMPERIUM-YYYY-YYYY-YYYY",
        style=discord.TextStyle.paragraph,
        max_length=2000,
    )
    action = ui.TextInput(
        label="Action: enable | disable",
        placeholder="enable  or  disable",
        style=discord.TextStyle.short,
        max_length=10,
    )

    async def on_submit(self, interaction: discord.Interaction):
        raw    = self.keys_input.value.strip().splitlines()
        keys   = [k.strip().upper() for k in raw if k.strip()]
        action = self.action.value.strip().lower()

        if action not in ("enable", "disable"):
            await interaction.response.send_message(
                "Action must be `enable` or `disable`.", ephemeral=True
            )
            return

        ok, fail = [], []
        for k in keys:
            result = disable_key(k) if action == "disable" else enable_key(k)
            (ok if result else fail).append(k)

        icon  = "🔴" if action == "disable" else "🟢"
        lines = []
        if ok:
            lines.append(f"**{icon} {action.capitalize()}d ({len(ok)}):**\n```\n" + "\n".join(ok) + "\n```")
        if fail:
            lines.append(f"**Not found ({len(fail)}):**\n```\n" + "\n".join(fail) + "\n```")

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay(f"## Manage Keys — {action.capitalize()}"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("\n\n".join(lines) or "Nothing processed."),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class HwidResetModal(ui.Modal, title="HWID Reset"):
    keys_input = ui.TextInput(
        label="Key(s) — one per line",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.paragraph,
        max_length=2000,
    )

    async def on_submit(self, interaction: discord.Interaction):
        raw  = self.keys_input.value.strip().splitlines()
        keys = [k.strip().upper() for k in raw if k.strip()]

        ok, fail = [], []
        for k in keys:
            (ok if reset_hwid(k) else fail).append(k)

        lines = []
        if ok:
            lines.append(f"**✅ HWID Reset ({len(ok)}):**\n```\n" + "\n".join(ok) + "\n```")
        if fail:
            lines.append(f"**Not found ({len(fail)}):**\n```\n" + "\n".join(fail) + "\n```")

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay("## HWID Reset"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                "\n\n".join(lines) + "\n\n"
                "HWID unbound. Login credentials preserved.\n"
                "HWID will re-bind on next launch."
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class UnclaimKeyModal(ui.Modal, title="Unclaim Key"):
    key_input = ui.TextInput(
        label="Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=KEY_LENGTH,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        rec = get_key(key)
        if not rec:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)
            return

        update_key(key,
            claimed_by_discord=None,
            username=None,
            password_hash=None,
            hwid=None,
            registered_at=None,
        )
        await interaction.response.send_message(
            f"✅ Key `{key}` has been **unclaimed**. It can now be registered again.",
            ephemeral=True,
        )


# ══════════════════════════════════════════════════════════════════════════════
#  Cog
# ══════════════════════════════════════════════════════════════════════════════

class AuthCog(commands.Cog):
    def __init__(self, bot: commands.Bot):
        self.bot = bot

    @app_commands.command(name="setdownload", description="Set the download link for /download. (Admin)")
    async def setdownload(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(SetDownloadModal())

    @app_commands.command(name="genkey", description="Generate one or more Imperium keys. (Admin)")
    async def genkey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(GenKeyModal())

    @app_commands.command(name="disablekey", description="Disable a key. (Admin)")
    async def disablekey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(DisableKeyModal())

    @app_commands.command(name="deletekey", description="Permanently delete a key. (Admin)")
    async def deletekey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(DeleteKeyModal())

    @app_commands.command(name="bulkdeletekeys", description="Delete multiple keys at once. (Admin)")
    async def bulkdeletekeys(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(BulkDeleteModal())

    @app_commands.command(name="managekey", description="View and edit a single key. (Admin)")
    async def managekey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(ManageKeyModal())

    @app_commands.command(name="managekeys", description="Enable or disable multiple keys. (Admin)")
    async def managekeys(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(ManageKeysModal())

    @app_commands.command(name="showallkeys", description="List all keys with full details. (Admin)")
    async def showallkeys(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return

        await interaction.response.defer(ephemeral=True)
        all_keys = get_all_keys()

        if not all_keys:
            await interaction.followup.send("No keys in the database.", ephemeral=True)
            return

        pages: list[str] = []
        current_lines: list[str] = []

        for key, rec in all_keys.items():
            gen_user     = "—"
            claimed_user = "—"
            if rec.get("generated_by"):
                m = interaction.guild.get_member(rec["generated_by"])
                gen_user = str(m) if m else f"ID:{rec['generated_by']}"
            if rec.get("claimed_by_discord"):
                m = interaction.guild.get_member(rec["claimed_by_discord"])
                claimed_user = str(m) if m else f"ID:{rec['claimed_by_discord']}"

            pw_raw     = rec.get("password_hash") or "—"
            pw_display = f"||{pw_raw}||" if pw_raw != "—" else "—"

            block = (
                f"```\n"
                f"Key      : {key}\n"
                f"Duration : {rec.get('duration', '—')}\n"
                f"Status   : {'DISABLED' if rec.get('disabled') else 'ACTIVE'}\n"
                f"GenBy    : {gen_user}\n"
                f"Claimed  : {claimed_user}\n"
                f"Username : {rec.get('username') or '—'}\n"
                f"HWID     : {rec.get('hwid') or 'unbound'}\n"
                f"Reg'd    : {(rec.get('registered_at') or '—')[:19]}\n"
                f"```\n"
                f"Password (spoiler): {pw_display}\n"
                f"{'─' * 30}"
            )
            current_lines.append(block)
            if len(current_lines) >= 5:
                pages.append("\n".join(current_lines))
                current_lines = []

        if current_lines:
            pages.append("\n".join(current_lines))

        total = len(all_keys)
        for i, page in enumerate(pages, 1):
            view = ui.LayoutView()
            view.add_item(ui.Container(
                ui.TextDisplay(f"## All Keys — Page {i}/{len(pages)}  ({total} total)\n\n{page}"),
                ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
                ui.TextDisplay("-# Imperium Bot — Key Management"),
            ))
            await interaction.followup.send(view=view, ephemeral=True)

    @app_commands.command(name="hwidreset", description="Reset HWID binding on one or more keys. (Admin)")
    async def hwidreset(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(HwidResetModal())

    @app_commands.command(name="unclaim", description="Unclaim a key so it can be registered again. (Admin)")
    async def unclaim(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(UnclaimKeyModal())

    @app_commands.command(name="register", description="Claim your key and create your loader account.")
    async def register(self, interaction: discord.Interaction):
        # No role gate — the key itself is the only requirement.
        # Founders can also register if they want a loader account.
        await interaction.response.send_modal(RegisterModal())

    @app_commands.command(name="download", description="Get the Imperium download link.")
    async def download(self, interaction: discord.Interaction):
        if not _is_customer(interaction.user):
            await interaction.response.send_message(
                "You need to register first. Use `/register` with your key.",
                ephemeral=True,
            )
            return

        url = await get_download_url()
        if not url:
            await interaction.response.send_message(
                "The download link hasn't been set yet. Contact an admin.",
                ephemeral=True,
            )
            return

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay("## Download Imperium"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                f"[**Download Latest Version**]({url})\n\n"
                f"Extract the `.rar` and run `Loader.exe`.\n"
                f"Log in with your **username** and **password**.\n"
                f"Your HWID binds automatically on first launch.\n\n"
                f"-# Do not share this link."
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium — Download"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


async def setup(bot: commands.Bot):
    await bot.add_cog(AuthCog(bot))
