"""
Imperium Bot — Auth / Key Management cog

Admin commands (Founder role only):
  /genkey             — generate keys via modal
  /disablekey         — disable a key
  /deletekey          — delete a key
  /bulkdeletekeys     — delete multiple keys
  /managekey          — view/edit a single key
  /managekeys         — enable/disable multiple keys
  /showallkeys        — list every key with full details
  /hwidreset          — reset HWID on one or more keys
  /unclaim            — unclaim a key (wipes credentials, keeps key)
  /setdownload        — manage product catalogue + download links
  /sethookloaderdll   — upload a new internal DLL for the hookloader

Customer commands:
  /register           — claim a key and create loader credentials
"""

import asyncio
import discord
from discord import app_commands, ui
from discord.ext import commands
import random
import string
from datetime import datetime, timezone

from cogs.database import (
    create_key, get_key, get_all_keys,
    disable_key, enable_key, delete_key, delete_keys,
    update_key, register_key, reset_hwid, key_exists,
    get_all_products, upsert_product, delete_product, set_product_global_url,
    upload_hookloader_dll,
)

# ─── Role IDs ─────────────────────────────────────────────────────────────────
FOUNDER_ROLE_ID  = 1511851579446137003
CUSTOMER_ROLE_ID = 1511852608858492938

KEY_PREFIX = "IMPERIUM"
KEY_LENGTH = 23

# channel_id → user_id: channels where an admin just clicked "Upload .dll File"
_pending_dll_uploads: dict[int, int] = {}


# ─── Helpers ──────────────────────────────────────────────────────────────────

def _gen_key() -> str:
    chars = string.ascii_uppercase + string.digits
    return KEY_PREFIX + "-" + "-".join(
        "".join(random.choices(chars, k=4)) for _ in range(3)
    )


def _is_founder(member: discord.Member) -> bool:
    return any(r.id == FOUNDER_ROLE_ID for r in member.roles)


def _validate_key_format(key: str) -> bool:
    parts = key.split("-")
    return (len(parts) == 4 and parts[0] == KEY_PREFIX
            and all(len(p) == 4 for p in parts[1:]))


def _build_product_panel_text() -> str:
    products = get_all_products()
    if not products:
        return "_No products configured yet. Use **Add Product** to create one._"
    lines = []
    for slug, info in products.items():
        name = info.get("display_name", slug)
        url  = info.get("url", "")
        url_display = f"[link]({url})" if url else "❌ _no link set_"
        lines.append(f"• **{name}** (`{slug}`) — {url_display}")
    return "\n".join(lines)


# ══════════════════════════════════════════════════════════════════════════════
#  /sethookloaderdll — panel + modals
# ══════════════════════════════════════════════════════════════════════════════

class HookloaderDllView(ui.View):
    """Panel shown by /sethookloaderdll."""
    def __init__(self, channel_id: int, user_id: int):
        super().__init__(timeout=300)
        self.channel_id = channel_id
        self.user_id    = user_id

    @ui.button(label="📁  Upload .dll File", style=discord.ButtonStyle.primary, row=0)
    async def upload_file(self, interaction: discord.Interaction, button: ui.Button):
        _pending_dll_uploads[self.channel_id] = self.user_id
        await interaction.response.send_message(
            "📎 Send your `.dll` file as an attachment **in this channel** now.\n"
            "The bot will pick it up and upload it to GitHub Releases automatically.",
            ephemeral=True,
        )

    @ui.button(label="🔗  Paste URL Instead", style=discord.ButtonStyle.secondary, row=0)
    async def paste_url(self, interaction: discord.Interaction, button: ui.Button):
        await interaction.response.send_modal(HookloaderUrlModal())


class HookloaderUrlModal(ui.Modal, title="Set Hookloader DLL URL"):
    url_input = ui.TextInput(
        label="Direct download URL for private.dll",
        placeholder="https://example.com/private.dll",
        style=discord.TextStyle.short,
        max_length=500,
    )

    async def on_submit(self, interaction: discord.Interaction):
        url = self.url_input.value.strip()
        if not url.startswith("http"):
            await interaction.response.send_message("❌ Invalid URL.", ephemeral=True)
            return
        from cogs.database import _load, _save
        data = _load()
        data["hookloader_dll_url"] = url
        _save(data)
        await interaction.response.send_message(
            f"✅ Hookloader DLL URL updated.\n`{url}`",
            ephemeral=True,
        )


# ══════════════════════════════════════════════════════════════════════════════
#  /setdownload — product download management panel
# ══════════════════════════════════════════════════════════════════════════════

class DownloadPanelView(ui.View):
    def __init__(self):
        super().__init__(timeout=300)

    def _embed(self) -> discord.Embed:
        return discord.Embed(
            title="📦  Product Download Manager",
            description=(
                "Manage the global product catalogue and their download links.\n"
                "Links are sent to the loader on authentication.\n"
                "Per-key overrides: `/addproducttokey`.\n\n"
                + _build_product_panel_text()
            ),
            color=0x1E90FF,
        ).set_footer(text="Imperium Bot — Download Management")

    @ui.button(label="✏️  Edit Link", style=discord.ButtonStyle.primary, row=0)
    async def edit_link(self, interaction: discord.Interaction, button: ui.Button):
        products = get_all_products()
        if not products:
            await interaction.response.send_message("No products yet. Add one first.", ephemeral=True)
            return
        await interaction.response.send_modal(EditProductLinkModal(list(products.keys())))

    @ui.button(label="➕  Add Product", style=discord.ButtonStyle.success, row=0)
    async def add_product(self, interaction: discord.Interaction, button: ui.Button):
        await interaction.response.send_modal(AddProductModal())

    @ui.button(label="🗑️  Remove Product", style=discord.ButtonStyle.danger, row=0)
    async def remove_product_btn(self, interaction: discord.Interaction, button: ui.Button):
        products = get_all_products()
        if not products:
            await interaction.response.send_message("No products yet.", ephemeral=True)
            return
        await interaction.response.send_modal(RemoveProductModal(list(products.keys())))

    @ui.button(label="🔄  Refresh", style=discord.ButtonStyle.secondary, row=0)
    async def refresh(self, interaction: discord.Interaction, button: ui.Button):
        await interaction.response.edit_message(embed=self._embed(), view=self)


class EditProductLinkModal(ui.Modal, title="Edit Product Download Link"):
    def __init__(self, slugs: list[str]):
        super().__init__()
        self.slug_input = ui.TextInput(
            label="Product slug",
            placeholder=" | ".join(slugs[:8]) or "e.g. imperium",
            style=discord.TextStyle.short, max_length=50,
        )
        self.url_input = ui.TextInput(
            label="Download URL",
            placeholder="https://example.com/product.rar",
            style=discord.TextStyle.short, max_length=500,
        )
        self.add_item(self.slug_input)
        self.add_item(self.url_input)

    async def on_submit(self, interaction: discord.Interaction):
        slug = self.slug_input.value.strip().lower()
        url  = self.url_input.value.strip()
        if not url.startswith("http"):
            await interaction.response.send_message("❌ Invalid URL.", ephemeral=True)
            return
        if slug not in get_all_products():
            await interaction.response.send_message(
                f"❌ Product `{slug}` not found. Use **Add Product** first.", ephemeral=True
            )
            return
        set_product_global_url(slug, url)
        if slug == "imperium":
            asyncio.create_task(set_download_url(url))
        view = DownloadPanelView()
        await interaction.response.edit_message(embed=view._embed(), view=view)


class AddProductModal(ui.Modal, title="Add / Update Product"):
    slug_input = ui.TextInput(
        label="Slug (internal ID, lowercase)",
        placeholder="e.g.  valorant2",
        style=discord.TextStyle.short, max_length=50,
    )
    name_input = ui.TextInput(
        label="Display Name",
        placeholder="e.g.  Valorant v2",
        style=discord.TextStyle.short, max_length=80,
    )
    url_input = ui.TextInput(
        label="Download URL (optional)",
        placeholder="https://example.com/product.rar",
        style=discord.TextStyle.short, max_length=500, required=False,
    )

    async def on_submit(self, interaction: discord.Interaction):
        slug = self.slug_input.value.strip().lower().replace(" ", "_")
        name = self.name_input.value.strip()
        url  = self.url_input.value.strip()
        if not slug or not name:
            await interaction.response.send_message("❌ Slug and name are required.", ephemeral=True)
            return
        if url and not url.startswith("http"):
            await interaction.response.send_message("❌ Invalid URL.", ephemeral=True)
            return
        upsert_product(slug, name, url)
        view = DownloadPanelView()
        await interaction.response.edit_message(embed=view._embed(), view=view)


class RemoveProductModal(ui.Modal, title="Remove Product"):
    def __init__(self, slugs: list[str]):
        super().__init__()
        self.slug_input = ui.TextInput(
            label="Product slug to remove",
            placeholder=" | ".join(slugs[:8]) or "e.g. csgo",
            style=discord.TextStyle.short, max_length=50,
        )
        self.add_item(self.slug_input)

    async def on_submit(self, interaction: discord.Interaction):
        slug = self.slug_input.value.strip().lower()
        if delete_product(slug):
            view = DownloadPanelView()
            await interaction.response.edit_message(embed=view._embed(), view=view)
        else:
            await interaction.response.send_message(f"❌ Product `{slug}` not found.", ephemeral=True)


# ══════════════════════════════════════════════════════════════════════════════
#  Key management modals
# ══════════════════════════════════════════════════════════════════════════════

class GenKeyModal(ui.Modal, title="Generate Key"):
    custom_key = ui.TextInput(
        label="Custom key (optional)",
        placeholder=f"{KEY_PREFIX}-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short, max_length=KEY_LENGTH, required=False,
    )
    duration = ui.TextInput(
        label="Duration",
        placeholder="e.g. 30d, lifetime",
        style=discord.TextStyle.short, max_length=50,
    )
    note = ui.TextInput(
        label="Note (optional)",
        style=discord.TextStyle.short, max_length=200, required=False,
    )
    quantity = ui.TextInput(
        label="Quantity (1–25)",
        placeholder="1",
        style=discord.TextStyle.short, max_length=2, default="1",
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
                    "❌ Custom key: quantity must be 1.", ephemeral=True
                )
                return
            if not _validate_key_format(custom):
                await interaction.response.send_message(
                    f"❌ Invalid key format. Must be `{KEY_PREFIX}-XXXX-XXXX-XXXX`.", ephemeral=True
                )
                return
            if key_exists(custom):
                await interaction.response.send_message(f"❌ Key `{custom}` already exists.", ephemeral=True)
                return
            create_key(custom, self.duration.value.strip(), interaction.user.id)
            generated.append(custom)
        else:
            for _ in range(qty):
                k = _gen_key()
                while key_exists(k):
                    k = _gen_key()
                create_key(k, self.duration.value.strip(), interaction.user.id)
                generated.append(k)

        note_line  = f"\n**Note:** {self.note.value.strip()}" if self.note.value.strip() else ""
        keys_block = "\n".join(generated)
        label      = "Key" if len(generated) == 1 else f"{len(generated)} Keys"

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay(f"## {label} Generated"),
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
        style=discord.TextStyle.short, max_length=KEY_LENGTH,
    )
    username_input = ui.TextInput(
        label="Username",
        placeholder="Choose a loader username",
        style=discord.TextStyle.short, max_length=32,
    )
    password_input = ui.TextInput(
        label="Password",
        placeholder="Choose a loader password",
        style=discord.TextStyle.short, max_length=64,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key      = self.key_input.value.strip().upper()
        username = self.username_input.value.strip()
        password = self.password_input.value.strip()

        if not _validate_key_format(key):
            await interaction.response.send_message(
                f"❌ Invalid key format. Must be `{KEY_PREFIX}-XXXX-XXXX-XXXX`.", ephemeral=True
            )
            return
        if not username or not password:
            await interaction.response.send_message("❌ Username and password cannot be empty.", ephemeral=True)
            return

        success, err = register_key(key, username, password, interaction.user.id)
        if not success:
            await interaction.response.send_message(f"❌ Registration failed: **{err}**", ephemeral=True)
            return

        role = interaction.guild.get_role(CUSTOMER_ROLE_ID)
        if role and role not in interaction.user.roles:
            try:
                await interaction.user.add_roles(role, reason="Registered Imperium key")
            except discord.Forbidden:
                pass

        await interaction.response.defer(ephemeral=True, thinking=False)

        reg_view = ui.LayoutView()
        reg_view.add_item(ui.Container(
            ui.TextDisplay("## ✅ Registration Successful"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                f"**Username:** `{username}`\n"
                f"**Password:** the one you just set\n"
                f"**Key:** `{key}`\n\n"
                f"-# Save these — you cannot recover your password."
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium — Registration"),
        ))
        await interaction.response.send_message(view=reg_view, ephemeral=True)

        guide_view = ui.LayoutView()
        guide_view.add_item(ui.Container(
            ui.TextDisplay("## 📖 Getting Started"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                "**Step 1** — Use `/download` to get `Loader.exe`.\n\n"
                "**Step 2** — Run it, enter your username and password.\n\n"
                "**Step 3** — Launch the game. The loader finds it automatically.\n\n"
                "**Step 4** — Press **INSERT** to open the menu.\n\n"
                "Your HWID binds on first launch. Changed PC? Ask an admin for `/hwidreset`."
            ),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium — Getting Started"),
        ))
        await interaction.followup.send(view=guide_view, ephemeral=True)


class DisableKeyModal(ui.Modal, title="Disable Key"):
    key_input = ui.TextInput(label="Key", placeholder="IMPERIUM-XXXX-XXXX-XXXX",
                              style=discord.TextStyle.short, max_length=KEY_LENGTH)

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        if disable_key(key):
            await interaction.response.send_message(f"🔴 Key `{key}` disabled.", ephemeral=True)
        else:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)


class DeleteKeyModal(ui.Modal, title="Delete Key"):
    key_input = ui.TextInput(label="Key", placeholder="IMPERIUM-XXXX-XXXX-XXXX",
                              style=discord.TextStyle.short, max_length=KEY_LENGTH)

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        if delete_key(key):
            await interaction.response.send_message(f"🗑️ Key `{key}` deleted.", ephemeral=True)
        else:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)


class BulkDeleteModal(ui.Modal, title="Bulk Delete Keys"):
    keys_input = ui.TextInput(
        label="Keys (one per line)",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX\nIMPERIUM-YYYY-YYYY-YYYY",
        style=discord.TextStyle.paragraph, max_length=2000,
    )

    async def on_submit(self, interaction: discord.Interaction):
        keys = [k.strip().upper() for k in self.keys_input.value.strip().splitlines() if k.strip()]
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
    key_input = ui.TextInput(label="Key", placeholder="IMPERIUM-XXXX-XXXX-XXXX",
                              style=discord.TextStyle.short, max_length=KEY_LENGTH)
    new_duration = ui.TextInput(label="New Duration (blank = keep)",
                                 style=discord.TextStyle.short, max_length=50, required=False)
    toggle_disabled = ui.TextInput(label="enable / disable / blank",
                                    placeholder="enable  or  disable",
                                    style=discord.TextStyle.short, max_length=10, required=False)

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        rec = get_key(key)
        if not rec:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)
            return
        changes, kwargs = [], {}
        if self.new_duration.value.strip():
            kwargs["duration"] = self.new_duration.value.strip()
            changes.append(f"Duration → `{self.new_duration.value.strip()}`")
        toggle = self.toggle_disabled.value.strip().lower()
        if toggle == "disable":
            kwargs["disabled"] = True;  changes.append("Status → 🔴 Disabled")
        elif toggle == "enable":
            kwargs["disabled"] = False; changes.append("Status → 🟢 Active")
        if kwargs:
            update_key(key, **kwargs)
        rec = get_key(key)
        gen_user = claimed_user = "—"
        if rec.get("generated_by"):
            m = interaction.guild.get_member(rec["generated_by"])
            gen_user = str(m) if m else f"ID:{rec['generated_by']}"
        if rec.get("claimed_by_discord"):
            m = interaction.guild.get_member(rec["claimed_by_discord"])
            claimed_user = str(m) if m else f"ID:{rec['claimed_by_discord']}"
        summary = (
            f"**Key:** `{key}`\n**Duration:** {rec.get('duration','—')}\n"
            f"**Status:** {'🔴 Disabled' if rec.get('disabled') else '🟢 Active'}\n"
            f"**Generated by:** {gen_user}\n**Claimed by:** {claimed_user}\n"
            f"**Username:** `{rec.get('username') or '—'}`\n"
            f"**HWID:** `{rec.get('hwid') or 'unbound'}`"
        )
        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay(f"## Manage Key — `{key}`"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(f"**Changes:**\n" + ("\n".join(changes) if changes else "None")),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(summary),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class ManageKeysModal(ui.Modal, title="Manage Multiple Keys"):
    keys_input = ui.TextInput(label="Keys (one per line)",
                               style=discord.TextStyle.paragraph, max_length=2000)
    action = ui.TextInput(label="Action: enable | disable",
                           style=discord.TextStyle.short, max_length=10)

    async def on_submit(self, interaction: discord.Interaction):
        keys   = [k.strip().upper() for k in self.keys_input.value.strip().splitlines() if k.strip()]
        action = self.action.value.strip().lower()
        if action not in ("enable", "disable"):
            await interaction.response.send_message("Action must be `enable` or `disable`.", ephemeral=True)
            return
        ok, fail = [], []
        for k in keys:
            (ok if (disable_key(k) if action == "disable" else enable_key(k)) else fail).append(k)
        icon = "🔴" if action == "disable" else "🟢"
        lines = []
        if ok:   lines.append(f"**{icon} {action.capitalize()}d ({len(ok)}):**\n```\n" + "\n".join(ok)   + "\n```")
        if fail: lines.append(f"**Not found ({len(fail)}):**\n```\n"                  + "\n".join(fail) + "\n```")
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
    keys_input = ui.TextInput(label="Key(s) — one per line",
                               style=discord.TextStyle.paragraph, max_length=2000)

    async def on_submit(self, interaction: discord.Interaction):
        keys = [k.strip().upper() for k in self.keys_input.value.strip().splitlines() if k.strip()]
        ok, fail = [], []
        for k in keys:
            (ok if reset_hwid(k) else fail).append(k)
        lines = []
        if ok:   lines.append(f"**✅ Reset ({len(ok)}):**\n```\n"       + "\n".join(ok)   + "\n```")
        if fail: lines.append(f"**Not found ({len(fail)}):**\n```\n"  + "\n".join(fail) + "\n```")
        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay("## HWID Reset"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("\n\n".join(lines) + "\n\nHWID unbound. Will re-bind on next launch."),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay("-# Imperium Bot — Key Management"),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


class UnclaimKeyModal(ui.Modal, title="Unclaim Key"):
    key_input = ui.TextInput(label="Key", placeholder="IMPERIUM-XXXX-XXXX-XXXX",
                              style=discord.TextStyle.short, max_length=KEY_LENGTH)

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        if not get_key(key):
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)
            return
        update_key(key, claimed_by_discord=None, username=None,
                   password_hash=None, hwid=None, registered_at=None)
        await interaction.response.send_message(
            f"✅ Key `{key}` unclaimed. It can be registered again.", ephemeral=True
        )


# ══════════════════════════════════════════════════════════════════════════════
#  Cog
# ══════════════════════════════════════════════════════════════════════════════

class AuthCog(commands.Cog):
    def __init__(self, bot: commands.Bot):
        self.bot = bot

    # ── /sethookloaderdll ─────────────────────────────────────────────────────
    @app_commands.command(
        name="sethookloaderdll",
        description="Upload a new DLL for the hookloader. (Admin)"
    )
    async def sethookloaderdll(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return

        current_url = get_all_products().get("private", {}).get("url", "") or "_(not set)_"
        embed = discord.Embed(
            title="🔧  Hookloader DLL Manager",
            description=(
                "Update the DLL that the hookloader downloads and injects.\n\n"
                "**Option 1 — Upload a file**\n"
                "Click **Upload .dll File**, then send your `.dll` as an attachment "
                "in this channel. The bot uploads it to GitHub Releases and updates "
                "the `private` product URL automatically.\n\n"
                "**Option 2 — Paste URL**\n"
                "If you already have a direct download URL, use **Paste URL Instead**.\n\n"
                f"**Current URL:**\n`{current_url}`"
            ),
            color=0x1E90FF,
        ).set_footer(text="Imperium Bot — Hookloader Management")

        view = HookloaderDllView(interaction.channel_id, interaction.user.id)
        await interaction.response.send_message(embed=embed, view=view, ephemeral=True)

    # ── /setdownload ──────────────────────────────────────────────────────────
    @app_commands.command(
        name="setdownload",
        description="Open the product download management panel. (Admin)"
    )
    async def setdownload(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        view = DownloadPanelView()
        await interaction.response.send_message(embed=view._embed(), view=view, ephemeral=True)

    # ── /genkey ───────────────────────────────────────────────────────────────
    @app_commands.command(name="genkey", description="Generate one or more keys. (Admin)")
    async def genkey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(GenKeyModal())

    # ── /disablekey ───────────────────────────────────────────────────────────
    @app_commands.command(name="disablekey", description="Disable a key. (Admin)")
    async def disablekey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(DisableKeyModal())

    # ── /deletekey ────────────────────────────────────────────────────────────
    @app_commands.command(name="deletekey", description="Permanently delete a key. (Admin)")
    async def deletekey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(DeleteKeyModal())

    # ── /bulkdeletekeys ───────────────────────────────────────────────────────
    @app_commands.command(name="bulkdeletekeys", description="Delete multiple keys. (Admin)")
    async def bulkdeletekeys(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(BulkDeleteModal())

    # ── /managekey ────────────────────────────────────────────────────────────
    @app_commands.command(name="managekey", description="View and edit a single key. (Admin)")
    async def managekey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(ManageKeyModal())

    # ── /managekeys ───────────────────────────────────────────────────────────
    @app_commands.command(name="managekeys", description="Enable or disable multiple keys. (Admin)")
    async def managekeys(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(ManageKeysModal())

    # ── /showallkeys ──────────────────────────────────────────────────────────
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

        pages, current_lines = [], []
        for key, rec in all_keys.items():
            gen_user = claimed_user = "—"
            if rec.get("generated_by"):
                m = interaction.guild.get_member(rec["generated_by"])
                gen_user = str(m) if m else f"ID:{rec['generated_by']}"
            if rec.get("claimed_by_discord"):
                m = interaction.guild.get_member(rec["claimed_by_discord"])
                claimed_user = str(m) if m else f"ID:{rec['claimed_by_discord']}"
            pw_raw     = rec.get("password_hash") or "—"
            pw_display = f"||{pw_raw}||" if pw_raw != "—" else "—"
            products   = ", ".join(rec.get("products", [])) or "none"
            block = (
                f"```\nKey      : {key}\n"
                f"Duration : {rec.get('duration','—')}\n"
                f"Status   : {'DISABLED' if rec.get('disabled') else 'ACTIVE'}\n"
                f"GenBy    : {gen_user}\nClaimed  : {claimed_user}\n"
                f"Username : {rec.get('username') or '—'}\n"
                f"HWID     : {rec.get('hwid') or 'unbound'}\n"
                f"Products : {products}\n"
                f"Reg'd    : {(rec.get('registered_at') or '—')[:19]}\n```\n"
                f"Password (spoiler): {pw_display}\n{'─'*30}"
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

    # ── /hwidreset ────────────────────────────────────────────────────────────
    @app_commands.command(name="hwidreset", description="Reset HWID on one or more keys. (Admin)")
    async def hwidreset(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(HwidResetModal())

    # ── /unclaim ──────────────────────────────────────────────────────────────
    @app_commands.command(name="unclaim", description="Unclaim a key so it can be registered again. (Admin)")
    async def unclaim(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(UnclaimKeyModal())

    # ── /register ─────────────────────────────────────────────────────────────
    @app_commands.command(name="register", description="Claim your key and create your loader account.")
    async def register(self, interaction: discord.Interaction):
        await interaction.response.send_modal(RegisterModal())

    # ── on_message — picks up .dll file uploads ───────────────────────────────
    @commands.Cog.listener()
    async def on_message(self, message: discord.Message):
        # Ignore bots
        if message.author.bot:
            return

        channel_id = message.channel.id
        if channel_id not in _pending_dll_uploads:
            return

        # Only accept from the user who triggered the command
        expected_user_id = _pending_dll_uploads[channel_id]
        if message.author.id != expected_user_id:
            return

        # Look for a .dll attachment
        dll_attachment = next(
            (a for a in message.attachments if a.filename.lower().endswith(".dll")),
            None,
        )
        if not dll_attachment:
            return

        # Consume the pending slot immediately so we don't double-process
        del _pending_dll_uploads[channel_id]

        status_msg = await message.reply(
            f"⏳ Uploading `{dll_attachment.filename}` to GitHub Releases…"
        )

        try:
            dll_bytes = await dll_attachment.read()
        except Exception as e:
            await status_msg.edit(content=f"❌ Failed to read attachment: {e}")
            return

        # Run the blocking upload in a thread so we don't block the event loop
        loop = asyncio.get_event_loop()
        success, result = await loop.run_in_executor(
            None, upload_hookloader_dll, dll_bytes
        )

        if success:
            await status_msg.edit(
                content=(
                    f"✅ DLL uploaded successfully!\n"
                    f"**Download URL:** <{result}>\n"
                    f"The `private` product URL has been updated. "
                    f"All keys with the `private` product assigned will get the new DLL on next launch."
                )
            )
        else:
            await status_msg.edit(content=f"❌ Upload failed: {result}")


async def setup(bot: commands.Bot):
    await bot.add_cog(AuthCog(bot))
