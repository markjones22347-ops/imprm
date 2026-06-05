"""
Imperium Bot — Product assignment cog

Admin commands (Founder role only):
  /addproducttokey    — assign products from the catalogue to a key (select menu)
  /removeproductkey   — remove a product from a key
  /viewkeyproducts    — view all products + links on a key
  /listproducts       — list the global product catalogue
  /listkeysproducts   — list every key and their assigned products
"""

import discord
from discord import app_commands, ui
from discord.ext import commands

from cogs.database import (
    get_all_keys,
    get_all_products,
    assign_products_to_key,
    set_product_link_on_key,
    get_products_for_key,
    remove_product_from_key,
    get_key,
)

FOUNDER_ROLE_ID = 1511851579446137003


def _is_founder(member: discord.Member) -> bool:
    return any(r.id == FOUNDER_ROLE_ID for r in member.roles)


# ══════════════════════════════════════════════════════════════════════════════
#  Add-product-to-key flow
#  Step 1: user types the key  →  Step 2: select menu of catalogue products
# ══════════════════════════════════════════════════════════════════════════════

class AddProductKeyModal(ui.Modal, title="Add Product to Key — Step 1"):
    """Ask for the key, then show the product select panel."""

    key_input = ui.TextInput(
        label="License Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=30,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key = self.key_input.value.strip().upper()
        rec = get_key(key)
        if not rec:
            await interaction.response.send_message(
                f"❌ Key `{key}` not found.", ephemeral=True
            )
            return

        products = get_all_products()
        if not products:
            await interaction.response.send_message(
                "❌ No products in the catalogue yet. "
                "Use `/setdownload` → **Add Product** to create some first.",
                ephemeral=True,
            )
            return

        view = ProductSelectView(key, rec, products)
        await interaction.response.send_message(
            embed=view._embed(), view=view, ephemeral=True
        )


class ProductSelectView(ui.View):
    """
    Shows the current products on a key and a Select menu to toggle them.
    The Select is pre-selected with what the key already has.
    """

    def __init__(self, key: str, rec: dict, catalogue: dict[str, dict]):
        super().__init__(timeout=300)
        self._key       = key
        self._rec       = rec
        self._catalogue = catalogue

        # Build Select options (max 25)
        options = []
        current = set(rec.get("products", []))
        for slug, info in list(catalogue.items())[:25]:
            options.append(
                discord.SelectOption(
                    label=info.get("display_name", slug),
                    value=slug,
                    description=f"slug: {slug}",
                    default=(slug in current),
                )
            )

        self._select = ui.Select(
            placeholder="Choose products for this key…",
            min_values=0,
            max_values=len(options),
            options=options,
        )
        self._select.callback = self._on_select
        self.add_item(self._select)

    def _embed(self) -> discord.Embed:
        rec      = get_key(self._key) or self._rec
        current  = rec.get("products", [])
        username = rec.get("username") or "Unclaimed"

        embed = discord.Embed(
            title=f"🔑  Add Products to Key",
            description=(
                f"**Key:** `{self._key}`\n"
                f"**User:** {username}\n\n"
                f"**Currently assigned:**\n"
                + (", ".join(f"`{p}`" for p in current) if current else "_none_")
                + "\n\n"
                "Use the dropdown below to choose which products this key has access to.\n"
                "Selecting products **replaces** the current list.\n"
                "To also set a **per-key download URL override**, use the button below."
            ),
            color=0x1E90FF,
        )
        catalogue = get_all_products()
        lines = []
        for slug, info in catalogue.items():
            url = info.get("url", "")
            lines.append(
                f"• **{info.get('display_name', slug)}** (`{slug}`) — "
                + (f"[global link]({url})" if url else "❌ no link")
            )
        if lines:
            embed.add_field(
                name="📦 Available Products (global catalogue)",
                value="\n".join(lines),
                inline=False,
            )
        embed.set_footer(text="Imperium Bot — Product Management")
        return embed

    async def _on_select(self, interaction: discord.Interaction):
        chosen = self._select.values  # list of slugs
        assign_products_to_key(self._key, chosen)

        # Refresh embed + keep view alive
        rec = get_key(self._key) or {}
        self._rec = rec
        # Update defaults on the select
        for opt in self._select.options:
            opt.default = opt.value in chosen

        await interaction.response.edit_message(embed=self._embed(), view=self)

    @ui.button(label="🔗  Set Per-Key URL Override", style=discord.ButtonStyle.secondary, row=1)
    async def set_url_override(self, interaction: discord.Interaction, button: ui.Button):
        catalogue = get_all_products()
        if not catalogue:
            await interaction.response.send_message("No products in catalogue.", ephemeral=True)
            return
        await interaction.response.send_modal(
            PerKeyUrlModal(self._key, list(catalogue.keys()))
        )

    @ui.button(label="✅  Done", style=discord.ButtonStyle.success, row=1)
    async def done(self, interaction: discord.Interaction, button: ui.Button):
        rec     = get_key(self._key) or {}
        current = rec.get("products", [])
        await interaction.response.edit_message(
            content=f"✅ Products saved for `{self._key}`: {', '.join(f'`{p}`' for p in current) or 'none'}",
            embed=None,
            view=None,
        )


class PerKeyUrlModal(ui.Modal, title="Set Per-Key URL Override"):
    """Override the download URL for one product on this specific key."""

    def __init__(self, key: str, product_slugs: list[str]):
        super().__init__()
        self._key = key
        hint = " | ".join(product_slugs[:8])
        self.slug_input = ui.TextInput(
            label="Product slug",
            placeholder=hint or "e.g. valorant",
            style=discord.TextStyle.short,
            max_length=50,
        )
        self.url_input = ui.TextInput(
            label="Download URL for this key",
            placeholder="https://example.com/special.rar",
            style=discord.TextStyle.short,
            max_length=500,
        )
        self.add_item(self.slug_input)
        self.add_item(self.url_input)

    async def on_submit(self, interaction: discord.Interaction):
        slug = self.slug_input.value.strip().lower()
        url  = self.url_input.value.strip()
        if not url.startswith("http"):
            await interaction.response.send_message("❌ Invalid URL.", ephemeral=True)
            return
        set_product_link_on_key(self._key, slug, url)
        await interaction.response.send_message(
            f"✅ Per-key URL set for `{slug}` on key `{self._key}`.",
            ephemeral=True,
        )


# ──────────────────────────────────────────────────────────────────────────────
#  Remove product from key — modal
# ──────────────────────────────────────────────────────────────────────────────

class RemoveProductKeyModal(ui.Modal, title="Remove Product from Key"):
    key_input = ui.TextInput(
        label="License Key",
        placeholder="IMPERIUM-XXXX-XXXX-XXXX",
        style=discord.TextStyle.short,
        max_length=30,
    )
    product_input = ui.TextInput(
        label="Product slug to remove",
        placeholder="e.g. valorant",
        style=discord.TextStyle.short,
        max_length=50,
    )

    async def on_submit(self, interaction: discord.Interaction):
        key     = self.key_input.value.strip().upper()
        product = self.product_input.value.strip().lower()

        if remove_product_from_key(key, product):
            await interaction.response.send_message(
                f"✅ Removed `{product}` from key `{key}`.", ephemeral=True
            )
        else:
            await interaction.response.send_message(
                f"❌ Key `{key}` not found or `{product}` not assigned.", ephemeral=True
            )


# ══════════════════════════════════════════════════════════════════════════════
#  Cog
# ══════════════════════════════════════════════════════════════════════════════

class ProductsCog(commands.Cog):
    def __init__(self, bot: commands.Bot):
        self.bot = bot

    # ── /addproducttokey ──────────────────────────────────────────────────────
    @app_commands.command(
        name="addproducttokey",
        description="Assign products from the catalogue to a license key. (Admin)",
    )
    async def addproducttokey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(AddProductKeyModal())

    # ── /removeproductkey ─────────────────────────────────────────────────────
    @app_commands.command(
        name="removeproductkey",
        description="Remove a product from a license key. (Admin)",
    )
    async def removeproductkey(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        await interaction.response.send_modal(RemoveProductKeyModal())

    # ── /viewkeyproducts ──────────────────────────────────────────────────────
    @app_commands.command(
        name="viewkeyproducts",
        description="View products and download links assigned to a key. (Admin)",
    )
    @app_commands.describe(key="The license key to inspect")
    async def viewkeyproducts(self, interaction: discord.Interaction, key: str):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return

        key = key.strip().upper()
        rec = get_key(key)
        if not rec:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)
            return

        products_data = get_products_for_key(key)
        catalogue     = get_all_products()

        embed = discord.Embed(
            title=f"Products for `{key}`",
            color=0x1E90FF,
        )
        embed.add_field(
            name="User",
            value=rec.get("username") or "Unclaimed",
            inline=True,
        )
        embed.add_field(
            name="Status",
            value="🔴 Disabled" if rec.get("disabled") else "🟢 Active",
            inline=True,
        )

        assigned = products_data["products"]
        if assigned:
            lines = []
            for p in assigned:
                per_key_url = products_data["links"].get(p, "")
                global_url  = catalogue.get(p, {}).get("url", "")
                display     = catalogue.get(p, {}).get("display_name", p)
                if per_key_url:
                    lines.append(f"• **{display}** (`{p}`) — [per-key link]({per_key_url}) _(override)_")
                elif global_url:
                    lines.append(f"• **{display}** (`{p}`) — [global link]({global_url})")
                else:
                    lines.append(f"• **{display}** (`{p}`) — ❌ no link set")
            embed.add_field(name="Assigned Products", value="\n".join(lines), inline=False)
        else:
            embed.add_field(name="Assigned Products", value="_None_", inline=False)

        embed.set_footer(text="Imperium Bot — Product Management")
        await interaction.response.send_message(embed=embed, ephemeral=True)

    # ── /listproducts ─────────────────────────────────────────────────────────
    @app_commands.command(
        name="listproducts",
        description="List all products in the global catalogue. (Admin)",
    )
    async def listproducts(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return

        catalogue = get_all_products()
        if not catalogue:
            await interaction.response.send_message(
                "No products in the catalogue. Use `/setdownload` → **Add Product**.",
                ephemeral=True,
            )
            return

        lines = []
        for slug, info in catalogue.items():
            name = info.get("display_name", slug)
            url  = info.get("url", "")
            url_display = f"[link]({url})" if url else "❌ _no link_"
            lines.append(f"• **{name}** (`{slug}`) — {url_display}")

        embed = discord.Embed(
            title="📦  Global Product Catalogue",
            description="\n".join(lines),
            color=0x1E90FF,
        )
        embed.set_footer(text="Edit links with /setdownload · Assign to keys with /addproducttokey")
        await interaction.response.send_message(embed=embed, ephemeral=True)

    # ── /listkeysproducts ─────────────────────────────────────────────────────
    @app_commands.command(
        name="listkeysproducts",
        description="List all keys and their assigned products. (Admin)",
    )
    async def listkeysproducts(self, interaction: discord.Interaction):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return

        await interaction.response.defer(ephemeral=True)
        keys = get_all_keys()
        if not keys:
            await interaction.followup.send("No keys found.", ephemeral=True)
            return

        embed = discord.Embed(
            title="🔑  Keys & Products",
            color=0x1E90FF,
        )
        for key, key_data in list(keys.items())[:25]:
            products = key_data.get("products", [])
            username = key_data.get("username") or "Unclaimed"
            embed.add_field(
                name=f"`{key[:8]}…`",
                value=f"**{username}** — " + (", ".join(f"`{p}`" for p in products) if products else "_none_"),
                inline=False,
            )
        if len(keys) > 25:
            embed.set_footer(text=f"Showing first 25 of {len(keys)} keys.")

        await interaction.followup.send(embed=embed, ephemeral=True)

    # ── Legacy aliases (old command names, kept for backward compat) ──────────
    @app_commands.command(name="assign_products", description="[Legacy] Use /addproducttokey instead.")
    @app_commands.describe(key="License key", products="Comma-separated slugs")
    async def assign_products(self, interaction: discord.Interaction, key: str, products: str):
        if not _is_founder(interaction.user):
            await interaction.response.send_message("Admin only.", ephemeral=True)
            return
        product_list = [p.strip().lower() for p in products.split(",")]
        if assign_products_to_key(key.strip().upper(), product_list):
            await interaction.response.send_message(
                f"✅ Products assigned to `{key}`: {', '.join(product_list)}", ephemeral=True
            )
        else:
            await interaction.response.send_message(f"❌ Key `{key}` not found.", ephemeral=True)


async def setup(bot: commands.Bot):
    await bot.add_cog(ProductsCog(bot))
