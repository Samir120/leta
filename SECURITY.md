# Security Policy

Thanks for taking the time to help make Leta safer.

## Supported versions

Leta is pre-release. During the pre-1.0 period, only the latest commit on `main` receives security
fixes.

Once `v1.0.0` ships, the policy will be:

| Version | Security fixes |
|---|---|
| Latest minor on the current major | ✅ |
| Older minors on the current major | ❌ |
| Previous majors | ❌ |

## Reporting a vulnerability

**Please do not open a public GitHub issue for a suspected vulnerability.**

Use GitHub's private vulnerability reporting on this repository:

> **Security → Advisories → Report a vulnerability**

You should receive an acknowledgement within **72 hours**. If you do not, please follow up.

When reporting, please include, as best you can:

- **Affected version** or commit hash
- **Environment** — OS, container runtime, whether the master key is set, whether Leta is behind
  a reverse proxy
- A **minimal reproduction** — the smallest sequence of requests, or the specific input file,
  that demonstrates the issue
- **Impact** — what an attacker could achieve
- Any **suggested fix** or mitigation, if you have one

Reports in English are preferred; Swedish is also fine.

## What is in scope

- The HTTP surface defined in [`docs/02-api-spec.yaml`](docs/02-api-spec.yaml)
- **Persistence and recovery.** WAL and snapshot readers must not crash, hang, or execute arbitrary
  code on any input, including truncated or corrupted files. Startup on adversarial on-disk data
  is a critical path.
- **Authentication and secret handling.** The master key is compared in constant time, never logged,
  never echoed in error responses, never used as a metrics label.
- **Container image.** Runs as non-root, no dynamic code loading, no unexpected outbound network
  calls.
- Memory-safety bugs in any parser (HTTP, JSON, query, WAL, snapshot).

## What is out of scope

- **Denial of service from missing rate limits or resource caps** when Leta is exposed directly to
  the public internet without an auth key or a reverse proxy. Leta is designed to sit behind a
  backend (see [`docs/02-api-guide.md`](docs/02-api-guide.md) §5); operating it otherwise is a
  misconfiguration, not a vulnerability.
- **Vulnerabilities in dependencies that are already public and being tracked.** Dependabot
  handles those; a duplicate report doesn't help.
- **Information disclosure through error messages when the master key is not set.** With no key,
  the server accepts all requests by design and logs a warning at startup.
- Missing security headers, CORS behaviour, TLS termination — Leta doesn't terminate TLS or serve
  browsers directly; those belong at the reverse proxy in front of it.

If you are unsure whether something is in scope, please report it privately and ask.

## Disclosure

- We aim to publish a fix or a mitigation within **90 days** of the initial report.
- With your permission, we will credit you in the release notes and the security advisory.
- If you would prefer to remain anonymous, we will honour that.
- If the issue is being actively exploited or is otherwise urgent, please say so in the report.

## Rewards

Leta is a solo side project without a budget for a bug bounty programme. We can offer public
acknowledgement and our sincere thanks — that's it. Please report responsibly anyway.
