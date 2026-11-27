# Google Sheets OAuth & Picker Setup Guide

gLabels Neo supports two ways to read live Google Sheets in **Data → Fill → Live data → Google Sheets…**:

1. **Public link (zero setup)** — Works immediately for any spreadsheet shared as *"Anyone with the link can view"*.
2. **Sign in with Google (any Gmail or Google Workspace user)** — Uses OAuth 2.0 (RFC 8252 loopback + PKCE) with the non-sensitive `drive.file` scope + Google Picker so any Google user can link private or Workspace spreadsheets and always print from the latest version without triggering Google's annual CASA security audit.

---

## One-Time Google Cloud Setup

### 1. Create a Google Cloud Project
1. Go to [Google Cloud Console](https://console.cloud.google.com/) and create a project (e.g. `glabels-neo`).

### 2. Enable APIs (*APIs & Services → Library*)
Enable these three APIs:
- **Google Sheets API**
- **Google Drive API**
- **Google Picker API**

### 3. Configure the OAuth Consent Screen (*Google Auth Platform → Branding / Audience / Data Access*)
- **App name:** `gLabels Neo`
- **User support email:** your contact email
- **Audience:** **External** (allows both personal Gmail and any Google Workspace account)
- **App home page:** `https://schneiderwastaken.github.io/glabels-neo/` (or your custom domain)
- **Privacy policy link:** `https://schneiderwastaken.github.io/glabels-neo/privacy/` (built automatically by `site/build.py`)
- **Authorized domains:** `github.io` (or your custom domain if using one)
- **Scopes (Data Access):**
  - `openid`
  - `email`
  - `https://www.googleapis.com/auth/drive.file` *(non-sensitive — avoids CASA audit)*
- **Test users:** Add your Google accounts while in **Testing** mode (note: refresh tokens expire after 7 days in Testing mode until you click **Publish App** and finish standard brand verification).

### 4. Create the Desktop OAuth Client ID (*Credentials → Create Credentials → OAuth client ID*)
- **Application type:** **Desktop app** (automatically allows `http://127.0.0.1:<port>/callback` loopback redirects)
- **Name:** `gLabels Neo Desktop`
- Save the generated **Client ID** and **Client Secret**.

### 5. Create the Google Picker Browser API Key (*Credentials → Create Credentials → API key*)
- Click **Edit API key**:
  - **Application restrictions:** **Websites** → add `https://schneiderwastaken.github.io/*` (and your custom domain if any)
  - **API restrictions:** Restrict key to **Google Picker API**
- Save the **API Key**.

---

## Wiring Into Builds & Releases

### GitHub Actions Release Builds (`.github/workflows/release.yml`)
Add these in **GitHub Repository Settings → Secrets and variables → Actions**:

| Kind | Name | Value |
| :--- | :--- | :--- |
| **Secret** | `GOOGLE_CLIENT_ID` | Your Desktop OAuth Client ID (`<projectNumber>-...apps.googleusercontent.com`) |
| **Secret** | `GOOGLE_CLIENT_SECRET` | Your Desktop OAuth Client Secret |
| **Variable** | `GOOGLE_PICKER_API_KEY` | Your website-restricted Google Picker API key |
| **Variable** *(optional)* | `GOOGLE_PICKER_URL` | Defaults to `https://schneiderwastaken.github.io/glabels-neo/picker/` |

Also ensure GitHub Pages is enabled (`PAGES_ENABLED = true`) so `/privacy/` and `/picker/` are live.

### Local Development / Testing Without Rebuilding
You can test Google Sign-In and Picker locally at any time by exporting environment variables before launching `glabels-qt`:

```powershell
$env:GLABELS_GOOGLE_CLIENT_ID     = "<your-client-id>"
$env:GLABELS_GOOGLE_CLIENT_SECRET = "<your-client-secret>"
$env:GLABELS_GOOGLE_PICKER_API_KEY = "<your-picker-api-key>"
# Optional: override picker URL if testing locally
# $env:GLABELS_GOOGLE_PICKER_URL  = "https://schneiderwastaken.github.io/glabels-neo/picker/"
```
