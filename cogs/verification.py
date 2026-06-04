"""
Imperium Bot — Verification cog
Commands: /setupverification
"""

import discord
from discord import app_commands, ui
from discord.ext import commands
import random

FOUNDER_ROLE_ID  = 1511851579446137003
VERIFIED_ROLE_ID = 1511852698725650482

# ─── Question pool ────────────────────────────────────────────────────────────
QUESTIONS: list[tuple[str, str]] = [
    ("14 + 8 = ?",   "22"),
    ("25 - 9 = ?",   "16"),
    ("6 x 7 = ?",    "42"),
    ("36 / 6 = ?",   "6"),
    ("19 + 24 = ?",  "43"),
    ("50 - 18 = ?",  "32"),
    ("8 x 4 = ?",    "32"),
    ("72 / 9 = ?",   "8"),
    ("33 + 17 = ?",  "50"),
    ("61 - 26 = ?",  "35"),
    ("5 x 9 = ?",    "45"),
    ("56 / 7 = ?",   "8"),
    ("42 + 15 = ?",  "57"),
    ("80 - 37 = ?",  "43"),
    ("7 x 8 = ?",    "56"),
    ("45 / 5 = ?",   "9"),
    ("28 + 36 = ?",  "64"),
    ("94 - 29 = ?",  "65"),
    ("3 x 11 = ?",   "33"),
    ("64 / 8 = ?",   "8"),
    ("16 + 27 = ?",  "43"),
    ("73 - 35 = ?",  "38"),
    ("9 x 6 = ?",    "54"),
    ("81 / 9 = ?",   "9"),
    ("22 + 18 = ?",  "40"),
    ("67 - 24 = ?",  "43"),
    ("4 x 12 = ?",   "48"),
    ("54 / 6 = ?",   "9"),
    ("31 + 29 = ?",  "60"),
    ("88 - 41 = ?",  "47"),
    ("10 x 5 = ?",   "50"),
    ("63 / 7 = ?",   "9"),
    ("17 + 34 = ?",  "51"),
    ("79 - 22 = ?",  "57"),
    ("8 x 9 = ?",    "72"),
    ("42 / 7 = ?",   "6"),
    ("26 + 15 = ?",  "41"),
    ("91 - 46 = ?",  "45"),
    ("7 x 7 = ?",    "49"),
    ("48 / 8 = ?",   "6"),
    ("13 + 28 = ?",  "41"),
    ("70 - 33 = ?",  "37"),
    ("11 x 4 = ?",   "44"),
    ("90 / 10 = ?",  "9"),
    ("24 + 37 = ?",  "61"),
    ("85 - 27 = ?",  "58"),
    ("6 x 12 = ?",   "72"),
    ("35 / 5 = ?",   "7"),
    ("18 + 19 = ?",  "37"),
    ("66 - 28 = ?",  "38"),
]


# ══════════════════════════════════════════════════════════════════════════════
#  Lock-down helper
#  - @everyone: view_channel=False on every category + channel
#  - @everyone: view_channel=True  on the verification channel only
#  - verified role: view_channel=True on every category + channel
# ══════════════════════════════════════════════════════════════════════════════

async def apply_verification_lockdown(
    guild: discord.Guild,
    verify_channel: discord.TextChannel,
    verified_role: discord.Role,
):
    everyone = guild.default_role

    # ── Categories ────────────────────────────────────────────────────────────
    for category in guild.categories:
        overwrites = dict(category.overwrites)
        overwrites[everyone]      = discord.PermissionOverwrite(view_channel=False)
        overwrites[verified_role] = discord.PermissionOverwrite(view_channel=True)
        try:
            await category.edit(overwrites=overwrites)
        except discord.Forbidden:
            pass

    # ── All channels ──────────────────────────────────────────────────────────
    for channel in guild.channels:
        if isinstance(channel, discord.CategoryChannel):
            continue  # already handled above

        overwrites = dict(channel.overwrites)

        if channel.id == verify_channel.id:
            # Verification channel — everyone can see it, verified can too
            overwrites[everyone]      = discord.PermissionOverwrite(view_channel=True)
            overwrites[verified_role] = discord.PermissionOverwrite(view_channel=True)
        else:
            overwrites[everyone]      = discord.PermissionOverwrite(view_channel=False)
            overwrites[verified_role] = discord.PermissionOverwrite(view_channel=True)

        try:
            await channel.edit(overwrites=overwrites)
        except discord.Forbidden:
            pass


# ══════════════════════════════════════════════════════════════════════════════
#  Setup modal — collects channel ID, panel text
# ══════════════════════════════════════════════════════════════════════════════

class VerificationSetupModal(ui.Modal, title="Setup Verification"):
    channel_id_input = ui.TextInput(
        label="Verification Channel ID",
        placeholder="Right-click the channel → Copy ID",
        style=discord.TextStyle.short,
        max_length=25,
    )
    panel_title = ui.TextInput(
        label="Panel Title",
        placeholder="e.g. Verify to access the server",
        style=discord.TextStyle.short,
        max_length=100,
        default="Verification",
    )
    panel_description = ui.TextInput(
        label="Panel Description",
        placeholder="Explain what members need to do to verify.",
        style=discord.TextStyle.paragraph,
        max_length=1000,
        default=(
            "To gain access to the server, click the button below and answer a simple "
            "maths question. Once you answer correctly you'll be verified instantly."
        ),
    )
    button_label_input = ui.TextInput(
        label="Verify Button Label",
        placeholder="e.g. Verify Me",
        style=discord.TextStyle.short,
        max_length=40,
        default="Verify Me",
    )

    async def on_submit(self, interaction: discord.Interaction):
        await interaction.response.defer(ephemeral=True, thinking=True)

        # Validate channel ID
        try:
            channel_id = int(self.channel_id_input.value.strip())
        except ValueError:
            await interaction.followup.send("Invalid channel ID — must be a number.", ephemeral=True)
            return

        channel = interaction.guild.get_channel(channel_id)
        if channel is None:
            await interaction.followup.send(
                f"Channel `{channel_id}` not found in this server.", ephemeral=True
            )
            return

        verified_role = interaction.guild.get_role(VERIFIED_ROLE_ID)
        if verified_role is None:
            await interaction.followup.send(
                f"Verified role `{VERIFIED_ROLE_ID}` not found. Make sure it exists in this server.",
                ephemeral=True,
            )
            return

        # Send the panel first
        panel_view = _build_panel(
            title=self.panel_title.value.strip(),
            description=self.panel_description.value.strip(),
            button_label=self.button_label_input.value.strip(),
        )
        await channel.send(view=panel_view)

        # Apply server-wide lockdown
        await apply_verification_lockdown(interaction.guild, channel, verified_role)

        await interaction.followup.send(
            f"Verification panel sent to {channel.mention} and server permissions updated.\n"
            f"Unverified members can only see {channel.mention}.",
            ephemeral=True,
        )


# ══════════════════════════════════════════════════════════════════════════════
#  Answer modal
# ══════════════════════════════════════════════════════════════════════════════

class VerificationAnswerModal(ui.Modal):
    answer_input = ui.TextInput(
        label="Your Answer",
        placeholder="Type the number only, e.g. 22",
        style=discord.TextStyle.short,
        max_length=10,
    )

    def __init__(self, question: str, correct: str):
        super().__init__(title=f"Verification — {question}")
        self.question = question
        self.correct  = correct

    async def on_submit(self, interaction: discord.Interaction):
        given = self.answer_input.value.strip()

        if given != self.correct:
            view = ui.LayoutView()
            view.add_item(ui.Container(
                ui.TextDisplay("## Incorrect"),
                ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
                ui.TextDisplay(
                    "That wasn't right. Click **Verify Me** again to get a new question.\n"
                    "-# Imperium Bot — Verification"
                ),
            ))
            await interaction.response.send_message(view=view, ephemeral=True)
            return

        # Correct — assign verified role
        verified_role = interaction.guild.get_role(VERIFIED_ROLE_ID)
        if verified_role is None:
            await interaction.response.send_message(
                "Verification role not found. Please contact an admin.", ephemeral=True
            )
            return

        if verified_role in interaction.user.roles:
            view = ui.LayoutView()
            view.add_item(ui.Container(
                ui.TextDisplay("## Already Verified"),
                ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
                ui.TextDisplay("You're already verified.\n-# Imperium Bot — Verification"),
            ))
            await interaction.response.send_message(view=view, ephemeral=True)
            return

        try:
            await interaction.user.add_roles(verified_role, reason="Passed verification")
        except discord.Forbidden:
            await interaction.response.send_message(
                "I don't have permission to assign that role. Please contact an admin.",
                ephemeral=True,
            )
            return

        view = ui.LayoutView()
        view.add_item(ui.Container(
            ui.TextDisplay("## Verified"),
            ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
            ui.TextDisplay(
                f"You've been verified and given the {verified_role.mention} role. Welcome!\n"
                f"-# Imperium Bot — Verification"
            ),
        ))
        await interaction.response.send_message(view=view, ephemeral=True)


# ══════════════════════════════════════════════════════════════════════════════
#  Panel builder
# ══════════════════════════════════════════════════════════════════════════════

def _build_panel(title: str, description: str, button_label: str) -> ui.LayoutView:
    view = ui.LayoutView()
    view.add_item(ui.Container(
        ui.TextDisplay(f"## {title}"),
        ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
        ui.TextDisplay(description),
        ui.Separator(visible=True, spacing=discord.SeparatorSpacing.small),
        ui.TextDisplay("-# Imperium Bot — Verification"),
    ))
    view.add_item(ui.ActionRow(
        ui.Button(
            label=button_label,
            style=discord.ButtonStyle.success,
            custom_id="verify_btn",
        )
    ))
    return view


# ══════════════════════════════════════════════════════════════════════════════
#  Cog
# ══════════════════════════════════════════════════════════════════════════════

class VerificationCog(commands.Cog):
    def __init__(self, bot: commands.Bot):
        self.bot = bot

    @commands.Cog.listener()
    async def on_interaction(self, interaction: discord.Interaction):
        if interaction.type != discord.InteractionType.component:
            return
        if interaction.data.get("custom_id") != "verify_btn":
            return

        q, a = random.choice(QUESTIONS)
        await interaction.response.send_modal(VerificationAnswerModal(question=q, correct=a))

    # ── /setupverification ────────────────────────────────────────────────────
    @app_commands.command(
        name="setupverification",
        description="Set up the verification panel and lock the server (Founder only).",
    )
    async def setupverification(self, interaction: discord.Interaction):
        founder_role = interaction.guild.get_role(FOUNDER_ROLE_ID)
        if founder_role not in interaction.user.roles:
            await interaction.response.send_message(
                "Only the Founder role can set up verification.", ephemeral=True
            )
            return
        await interaction.response.send_modal(VerificationSetupModal())


async def setup(bot: commands.Bot):
    await bot.add_cog(VerificationCog(bot))
