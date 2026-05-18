# Claude Usage Desk Display Setup

This display shows your Claude API monthly usage cost, GitHub service status, and basic device information.

Your display has already been flashed and assembled. You only need to connect it to Wi-Fi and provision it with your Claude analytics details.

## What You Need

- The desk display.
- A USB power source.
- A laptop on the Wi-Fi network you want the display to use.
- Node.js 20 or newer installed on your laptop.
- Your Claude email address.
- An Anthropic analytics API key with permission to read organization analytics.

## Setup

### 1. Start the helper app

On your laptop, open a terminal in the supplied `helper` folder and run:

```powershell
cd ./helper
npm install
npm start
```

Then open this page in your browser:

```text
http://localhost:3333/setup
```

Enter:

- your Anthropic analytics API key
- your Claude email address

When setup succeeds, the helper will show a ready page:

```text
http://localhost:3333/ready
```

Keep this helper page open until the display finishes provisioning or starts showing usage.

The ready page also shows a 6-digit setup code. You will need this code in the next step. The code changes each time the helper app is restarted.

### 2. Connect to the display setup Wi-Fi

Plug the display into USB power.

If it has not been set up before, it will create a Wi-Fi setup network called:

```text
DeskDisplay-Setup
```

On your laptop or phone, open Wi-Fi settings and connect to `DeskDisplay-Setup`.

Your device may warn that this network has no internet. That is expected. Stay connected to it for setup.

A setup page should open automatically. If it does not, open:

```text
http://192.168.4.1
```

Choose your normal Wi-Fi network, enter the Wi-Fi password, and leave the helper hostname as:

```text
desk-display.local
```

Enter the 6-digit setup code shown on the helper ready page.

Save the setup.

After saving, the display will leave `DeskDisplay-Setup` and join your normal Wi-Fi. Your laptop or phone can reconnect to your normal Wi-Fi as well.

### 3. Wait for provisioning

The display will join your Wi-Fi and contact the helper running on your laptop.

The helper only provisions displays that provide the matching setup code. This prevents another display on the same Wi-Fi from accidentally copying your setup.

Once provisioning completes, the display stores what it needs locally and starts fetching usage by itself.

You can then close the helper app and browser page.

The helper is only needed for first-time setup or if you reset the display later.

## Using The Display

Press the button to switch screens:

- Usage: your current monthly Claude usage.
- GitHub: current GitHub service status.
- Info: Wi-Fi status, IP address, and configured Claude email.

Hold the button for about 3 seconds to reset the display for a different user or Wi-Fi network.

## Resetting The Display

To give the display to someone else, move it to a different Wi-Fi network, or re-enter your API details:

1. Hold the button for about 3 seconds.
2. Wait for the display to restart.
3. Connect to `DeskDisplay-Setup`.
4. Run the setup steps again.

This clears the saved Wi-Fi, Claude email, API key, and user ID from the display.

## Troubleshooting

### I cannot see `DeskDisplay-Setup`

Hold the button for 3 seconds to reset the setup state, then wait for the display to restart.

If it still does not appear, unplug and reconnect the display.

### The display says to run the helper app

The display could not reach the helper on your laptop.

Check:

- `npm start` is still running.
- The helper ready page is open at `http://localhost:3333/ready`.
- Your laptop and display are on the same Wi-Fi after setup.
- Your firewall allows Node.js to accept local network connections.
- The setup code entered in the display setup portal matches the code on the helper ready page.

If discovery fails, the helper ready page shows fallback helper addresses such as:

```text
http://192.168.1.50:3333
```

Reset the display, go through Wi-Fi setup again, and enter only the IP address part in the helper hostname field:

```text
192.168.1.50
```

Do not include `http://` or `:3333`.

Use the current 6-digit setup code from the helper ready page. If you restarted the helper, the old code will no longer work.

### The helper says the API key cannot read analytics

The key does not have the required analytics permission, or it cannot see usage for the email address you entered.

Check that you are using the right Claude email address and an Anthropic analytics API key with organization analytics access.

### The display shows 0 percent usage

This can happen if:

- you have no Claude usage for the current month
- the wrong email address was entered
- the API key cannot see your user

Reset the display and run setup again if the email or key might be wrong.

### GitHub status is orange

The GitHub screen turns orange when GitHub reports a degraded service or when the display cannot check GitHub status. The affected area or error should appear below the indicator.

## Privacy And Credentials

During setup, the helper stores your API key in your laptop's secure keychain. The display stores the API key locally after provisioning so it can update without the helper running.

Treat the display like a device containing a personal API credential. Hold the button for 3 seconds to reset it before giving it to someone else.
