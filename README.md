# Backdrop Federation

Enables ActivityPub federation for Backdrop CMS, connecting your site to the Fediverse. Fediverse users can follow the site and receive posts in their feeds. Replies, likes, and boosts from the Fediverse are reflected back on the site.

The core module implements an **outbound publisher** model: the site acts as an ActivityPub actor that broadcasts content to followers. An optional sub-module, **Backdrop Federation Follow**, extends this to two-way federation by allowing the site to follow remote accounts and receive their posts in a local feed. A "Fediverse Feed" block is available to display the incoming feed.

---

## Status

We have installed and tested the module under limited server conditions. Other than the issues reported in the issue queue, we expect that everything is working. However, this module needs additional testing and feedback.

- We would really appreciate some feedback on the user interface.
- The blocklist feature needs additional testing.
- We anticipate some people may experience problems depending upon server configuration. Let us know about your experiences.

---

## Limitations

This module federates a content-publishing site rather than running a social network, which shapes what it can and cannot do.

The most significant constraint is the **single-actor model** — the entire site presents itself as one identity, so individual authors have no Fediverse presence of their own. Only a subset of ActivityPub's activity vocabulary is supported: the site cannot originate likes, boosts, or replies, and outgoing posts lack hashtag and mention metadata, limiting their discoverability on remote servers. Everything published is effectively public, with no support for followers-only, unlisted, or direct-message visibility.

Posts received from followed accounts are displayed more plainly than in a native Fediverse client. Inline images are rendered, but link previews and OpenGraph images are not fetched, so posts that depend on a rich preview card will appear as bare text links.

Finally, delivery is best-effort rather than guaranteed — if a remote inbox is down at publish time and cron misses it, the post is not delivered.

For a site whose primary purpose is publishing content to an audience that includes Fediverse users, these are reasonable trade-offs. For anything resembling a social platform, they would be fundamental gaps.

---

## Installation

Install this module using the official Backdrop CMS instructions. Bugs and feature requests should be reported in the Issue Queue.

---

## Getting Started

### 1. Configure the site actor

Go to **Administration › Configuration › Web services › Backdrop Federation** and open the **Settings** tab.

- Set an **actor username** — this becomes the local part of your Fediverse handle (e.g. `news` gives you `@news@yoursite.com`)
- Add a **display name** and **bio** that will appear on your profile when viewed from Mastodon or other Fediverse apps
- Select which **content types** should be federated — only published nodes of these types will be broadcast to followers
- Save the settings

### 2. Verify your handle is discoverable

Your site's Fediverse handle is `@username@yourdomain.com`. Search for it from any Mastodon account — if your profile appears, WebFinger is working correctly and the site is ready to federate.

> **Note:** The site must be publicly accessible over HTTPS. Federation will not work on localhost or behind a firewall.

### 3. Follow the site and publish a post

Follow your site's handle from a Mastodon or other Fediverse account. The follower should appear under the **Followers** tab within a few seconds. Then publish or re-save a node of a federated content type and check that it appears in your Fediverse feed.

> **Tip:** If you published content before any followers existed, use the **Re-deliver** button on the Settings tab to re-send all outbox entries to your current followers.

### 4. Try the Follow sub-module (optional)

Enable **Backdrop Federation Follow** to follow remote Fediverse accounts and receive their posts on your site. Once enabled, two new tabs appear on the settings page:

- **Following** — enter a `@user@domain` handle and click Follow; the module resolves the handle and sends a follow request automatically
- **Feed** — browse posts received from followed accounts

To display the feed on your site, add the **Fediverse Following Feed** block to any layout region via **Structure › Layouts**. You can also use the **Views** module to build a custom public-facing feed page using the "Fediverse Feed Post" base table.

---

## Included Sub-modules

| Sub-module | Purpose |
|---|---|
| **Backdrop Federation Follow** | Follow remote Fediverse accounts and display their posts in a local feed |

Sub-modules are located inside the `backdrop_federation/` directory and can be enabled independently at `admin/modules`.

---

## Features

- **Site actor** — the site presents itself as an ActivityPub `Application` actor discoverable by `@username@domain` handle
- **WebFinger** — resolves `acct:name@domain` handles so remote servers can discover the actor
- **NodeInfo** — advertises software name, version, and usage statistics to the Fediverse
- **Content federation** — selected content types are automatically broadcast when nodes are published, updated, or unpublished
- **Immediate delivery** — posts are delivered to follower inboxes synchronously on node save, with cron as a fallback
- **Per-type field mapping** — choose which text field is used as the post body for each content type
- **Per-type object type** — federate each content type as a `Note` or `Article`
- **Content negotiation** — node URLs respond with ActivityPub JSON when requested with an appropriate `Accept` header, allowing remote servers to fetch and verify notes by URL
- **Follower management** — accepts `Follow` activities, stores followers, and sends signed `Accept` responses
- **Shared inbox support** — uses a follower's shared inbox when available to reduce delivery requests
- **Re-deliver button** — re-queues all outbox entries to current followers from the admin UI
- **Outbox debug view** — browse outgoing activities and inspect their raw JSON from the admin UI
- **Replies as comments** — incoming Fediverse replies to local nodes are optionally saved as Backdrop comments
- **Comment moderation** — Fediverse replies can be held for approval before publishing
- **Likes and boosts** — incoming `Like` and `Announce` activities are counted and displayed on nodes
- **HTTP Signatures** — all outgoing requests are signed; all incoming requests are verified (draft-cavage)
- **Blocklist** — block specific actors by URI or entire domains; blocked followers are also removed
- **Fediverse handle support** — blocklist accepts `@user@domain` handles in addition to raw actor URIs
- **Rate limiting** — per-actor inbox rate limiting with configurable window and request limit
- **Subscribe page** — a form at `/fediverse/subscribe` lets visitors initiate a remote follow from their own instance
- **RSA keypair** — a 2048-bit keypair is generated on install and stored in the database
- **Views integration** — the followers table is exposed to the Views module for building custom displays

---

## Configuration

All settings are at **Administration › Configuration › Web services › Backdrop Federation** (`admin/config/services/fediverse`).

| Tab | Purpose |
|---|---|
| Settings | Enable/disable federation, actor username, display name, bio, content types, reply handling |
| Followers | List of current Fediverse followers |
| Outbox | Browse outgoing activities and inspect raw JSON (debug) |
| Blocklist | Block actors or domains; accepts raw URIs or `@user@domain` handles |
| Rate Limit | Max requests per actor per time window |

---

## How It Works

See [Technical_information.md](Technical_information.md) for a detailed breakdown of each subsystem: actor discovery, content federation, delivery, inbox handling, HTTP signatures, blocklist, and rate limiting.

---

## Sub-module: Backdrop Federation Follow

The **Backdrop Federation Follow** sub-module adds two-way federation: the site can follow remote Fediverse accounts and receive their posts into a local feed.

### Features

- Follow any Fediverse account by entering a `@user@domain` handle in the admin UI
- Resolves handles via WebFinger and sends a signed `Follow` activity automatically
- Tracks follow status (Pending, Accepted, Rejected) as remote servers respond
- Stores incoming posts from followed accounts in a local feed
- Feed posts are updated or removed when the remote account edits or deletes them
- Unfollow sends a signed `Undo Follow` activity and removes the account's posts from the local feed
- **Pause / Resume** — pause a followed account to temporarily stop receiving their posts without losing the record; resume re-sends a `Follow` and resumes delivery
- **Boosts** — optionally include boosted posts in the local feed (configurable globally)
- **Feed block** — a "Fediverse Following Feed" block for placement in Backdrop layouts
- **Views integration** — both the feed posts and followed accounts tables are exposed to the Views module for building custom displays, including public-facing feed pages

### Configuration

Two additional tabs appear at `admin/config/services/fediverse` when the sub-module is enabled:

| Tab | Purpose |
|---|---|
| Following | Manage followed accounts; follow, pause, resume, or unfollow |
| Feed | Browse posts received from followed accounts (paginated) |

Feed settings (retention period, boost inclusion) are on the Following tab.

### Fediverse Following Feed Block

A **Fediverse Following Feed** block is available for placement in any Backdrop layout region. Each block instance has its own settings:

| Setting | Description |
|---|---|
| Number of posts | How many recent posts to display (5, 10, 15, 20, 25, or 50) |
| Show avatars | Display the poster's avatar next to each post |
| Truncate content | Optionally truncate post text (no limit, 140, 280, or 500 characters) |
| Max image width / height | Cap the size of inline images (100–400 px) |

- Content is sanitized with `filter_xss` at ingest time and output directly in the block
- Hashtag links are converted to plain text; regular hyperlinks are preserved
- Boosts show the booster's name and avatar alongside the original post
- A "View all posts" link at the bottom of the block links to the admin feed page

### How It Works

1. Admin enters a handle at the **Following** tab
2. The module resolves the handle via WebFinger to get the actor URI and inbox
3. A signed `Follow` activity is POSTed to the remote inbox
4. The remote server sends `Accept` or `Reject` back to `/fediverse/inbox`
5. On `Accept`, the follow status is updated and the remote server begins delivering posts
6. Incoming `Create` activities from followed accounts are stored in `backdrop_federation_feed`
7. `Update` and `Delete` activities from followed accounts update or remove the stored posts
8. `Announce` (boost) activities optionally add the boosted post to the feed; `Undo Announce` removes it

### Developer Hook

The parent module invokes `hook_backdrop_federation_inbox_activity($activity)` for every verified incoming activity. The Follow sub-module uses this to handle `Accept`, `Reject`, `Create`, `Update`, `Delete`, `Announce`, and `Undo` activities from followed accounts without modifying the parent module's inbox handler. Other modules can implement this hook to react to any incoming ActivityPub activity.

---

## Database Tables

### Core module

| Table | Purpose |
|---|---|
| `backdrop_federation_keypair` | RSA keypair for signing |
| `backdrop_federation_followers` | Remote followers and their inbox URIs |
| `backdrop_federation_outbox` | Outgoing activities |
| `backdrop_federation_received` | Incoming objects mapped to comment IDs |
| `backdrop_federation_reactions` | Like and Announce counts per node |
| `backdrop_federation_blocklist` | Blocked actors and domains |
| `backdrop_federation_rate_limit` | Per-actor request counts for rate limiting |

### Backdrop Federation Follow sub-module

| Table | Purpose |
|---|---|
| `backdrop_federation_following` | Remote accounts this site follows, with status and inbox URIs |
| `backdrop_federation_feed` | Posts received from followed accounts |

---

## ActivityPub Endpoints

All endpoints are publicly accessible. The URL paths use `/fediverse/` regardless of module name for compatibility with remote servers that have stored these URLs.

| Path | Purpose |
|---|---|
| `/.well-known/webfinger` | Actor discovery |
| `/.well-known/nodeinfo` | NodeInfo discovery |
| `/nodeinfo/2.1` | NodeInfo metadata |
| `/fediverse/actor` | Actor document |
| `/fediverse/inbox` | Incoming activities |
| `/fediverse/outbox` | Published activities |
| `/fediverse/followers` | Followers collection |
| `/fediverse/subscribe` | Remote follow form |

---

## Not Yet Implemented

The following are standard or commonly expected ActivityPub features not currently present in this module.

**Missing endpoints / collections**
- **`/fediverse/following`** — An empty collection endpoint should exist on the actor even when the Follow sub-module is not enabled; many servers expect it to be present
- **`/fediverse/liked`** — Collection of objects the actor has liked; less critical but part of the spec

**Missing actor properties**
- **`endpoints.sharedInbox`** — The actor document does not advertise its own shared inbox URI. The module already uses shared inboxes when delivering to followers, but does not expose one for incoming traffic
- **`featured`** — A collection of pinned posts, used by Mastodon to show highlighted content on the profile

**Missing content features**
- **Hashtags** — Posts should include a `tag` array with `Hashtag` objects so they appear in remote tag feeds (e.g. `#backdrop` on Mastodon)
- **Mentions** — `@user@instance` references in post content should be expressed as `tag` objects of type `Mention`
- **Content warnings** — The `summary` field on Note/Article objects is populated from the node teaser but is not exposed as a configurable content warning field

**Missing Follow sub-module features**
- **Public feed page** — the feed is currently admin-only by default; a public-facing page can be built using the Views integration, but no pre-configured view is provided out of the box

---

## AI Statement

AI tools were used in the creation of this module with human supervision and testing.
Please use with caution and be sure to report any errors or suspicious code.

## Current Maintainer

Tim Erickson (@stpaultim)

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
