"""
Product Management Cog
Allows admins to assign products to license keys and manage download links.
"""

import discord
from discord.ext import commands
from discord import app_commands
from cogs.database import (
    get_all_keys,
    assign_products_to_key,
    set_product_link,
    get_products_for_key,
    remove_product_from_key,
)


class ProductsCog(commands.Cog):
    def __init__(self, bot):
        self.bot = bot
        self.available_products = ["emu", "popup", "valorant", "csgo"]  # Define available products

    @app_commands.command(name="assign_products", description="Assign products to a license key")
    @app_commands.describe(
        key="The license key to assign products to",
        products="Comma-separated product names (emu, popup, valorant, csgo, etc.)",
    )
    async def assign_products(self, interaction: discord.Interaction, key: str, products: str):
        """Assign one or more products to a license key (admin only)."""
        if not interaction.user.guild_permissions.administrator:
            await interaction.response.send_message(
                "❌ You need administrator permissions to use this command.",
                ephemeral=True,
            )
            return

        product_list = [p.strip().lower() for p in products.split(",")]
        
        if assign_products_to_key(key, product_list):
            product_str = ", ".join(product_list)
            await interaction.response.send_message(
                f"✅ Assigned products to key `{key}`: {product_str}",
                ephemeral=False,
            )
        else:
            await interaction.response.send_message(
                f"❌ Key `{key}` not found.",
                ephemeral=True,
            )

    @app_commands.command(name="set_product_link", description="Set download link for a product on a key")
    @app_commands.describe(
        key="The license key",
        product="Product name (emu, popup, etc.)",
        url="Download URL for the product",
    )
    async def set_product_link(self, interaction: discord.Interaction, key: str, product: str, url: str):
        """Set or update the download link for a specific product on a key (admin only)."""
        if not interaction.user.guild_permissions.administrator:
            await interaction.response.send_message(
                "❌ You need administrator permissions to use this command.",
                ephemeral=True,
            )
            return

        if set_product_link(key, product.lower(), url):
            await interaction.response.send_message(
                f"✅ Set download link for `{product}` on key `{key}`",
                ephemeral=False,
            )
        else:
            await interaction.response.send_message(
                f"❌ Key `{key}` not found.",
                ephemeral=True,
            )

    @app_commands.command(name="view_key_products", description="View products assigned to a key")
    @app_commands.describe(key="The license key to query")
    async def view_key_products(self, interaction: discord.Interaction, key: str):
        """View all products and download links assigned to a key."""
        if not interaction.user.guild_permissions.administrator:
            await interaction.response.send_message(
                "❌ You need administrator permissions to use this command.",
                ephemeral=True,
            )
            return

        products_data = get_products_for_key(key)
        
        if not products_data["products"]:
            await interaction.response.send_message(
                f"❌ Key `{key}` not found or has no products assigned.",
                ephemeral=True,
            )
            return

        embed = discord.Embed(
            title=f"Products for Key: {key}",
            color=discord.Color.green(),
        )

        products_str = ", ".join(products_data["products"]) if products_data["products"] else "None"
        embed.add_field(name="Products", value=products_str, inline=False)

        links_str = ""
        for product, url in products_data["links"].items():
            links_str += f"**{product}**: {url}\n"
        if links_str:
            embed.add_field(name="Download Links", value=links_str, inline=False)
        else:
            embed.add_field(name="Download Links", value="None configured", inline=False)

        await interaction.response.send_message(embed=embed, ephemeral=False)

    @app_commands.command(name="remove_product", description="Remove a product from a key")
    @app_commands.describe(key="The license key", product="Product name to remove")
    async def remove_product(self, interaction: discord.Interaction, key: str, product: str):
        """Remove a product from a key's product list (admin only)."""
        if not interaction.user.guild_permissions.administrator:
            await interaction.response.send_message(
                "❌ You need administrator permissions to use this command.",
                ephemeral=True,
            )
            return

        if remove_product_from_key(key, product.lower()):
            await interaction.response.send_message(
                f"✅ Removed `{product}` from key `{key}`",
                ephemeral=False,
            )
        else:
            await interaction.response.send_message(
                f"❌ Key `{key}` not found or product `{product}` not assigned.",
                ephemeral=True,
            )

    @app_commands.command(name="list_keys_with_products", description="List all keys and their assigned products")
    async def list_keys_with_products(self, interaction: discord.Interaction):
        """Show all keys with their assigned products (admin only)."""
        if not interaction.user.guild_permissions.administrator:
            await interaction.response.send_message(
                "❌ You need administrator permissions to use this command.",
                ephemeral=True,
            )
            return

        await interaction.response.defer()

        keys = get_all_keys()
        if not keys:
            await interaction.followup.send("❌ No keys found.", ephemeral=True)
            return

        embed = discord.Embed(
            title="All Keys and Products",
            color=discord.Color.blue(),
        )

        for key, key_data in keys.items():
            products = key_data.get("products", [])
            username = key_data.get("username", "Unclaimed")
            product_str = ", ".join(products) if products else "None"
            embed.add_field(
                name=f"Key: {key[:8]}...",
                value=f"**User**: {username}\n**Products**: {product_str}",
                inline=False,
            )

        if len(embed.fields) > 0:
            await interaction.followup.send(embed=embed, ephemeral=False)
        else:
            await interaction.followup.send("❌ No keys with product information found.", ephemeral=True)


async def setup(bot):
    await bot.add_cog(ProductsCog(bot))
