# Release notes (English, for the store's "Release Notes" field)

## 1.3

- Toggl's hourly request quota (30 requests per hour on the free plan) is now handled properly: a "402" reply used to show up as "not in your plan", now the app pauses exactly until the quota resets and tells you how long
- Polling while the app is open is slower (every 3 minutes) and stops early to keep a few requests for start/stop
- Fewer requests overall: starting a timer costs one request, stopping one (rounding included), a refresh one; reopening the app within 90 seconds uses the cached status, projects are cached for a day and entries for 15 minutes
- Timeline pin errors back off for 15 minutes instead of retrying on every update
- The diagnostics block is gone from the settings page

## 1.2

- Timeline pin for the running timer, with a "Stop timer" action right on the pin (can be turned off in the settings)
- Far fewer Toggl API requests: projects are cached for hours and entries for minutes, starting and stopping cost one or two requests, and the app pauses for five minutes when Toggl reports its rate limit
- Commands sent while the app is still loading are carried out afterwards instead of being dropped
- Sample data in the demo mode ("demo" as token) is now neutral

## 1.1

- First store release
- Status screen with project, client and tags, running time and daily total
- Favourite tiles, recent entries and project list, voice dictation
- Touch and swipe control on the Pebble Time 2
- Reminders for forgotten timers and idle mornings, optional rounding
- Languages: English, German, French, Italian, Spanish
