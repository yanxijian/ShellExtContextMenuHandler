# Security Policy

**English** | [简体中文](SECURITY.md)

## Supported versions

| Version | Supported |
|---------|-----------|
| 1.0.x   | Yes       |
| Older   | No        |

## Reporting a vulnerability

Please **do not** open a public issue for security-sensitive reports.

Prefer one of:

1. GitHub Security Advisories: https://github.com/yanxijian/ShellExtContextMenuHandler/security/advisories/new
2. Contact the maintainer through the GitHub profile: https://github.com/yanxijian

Include:

- Affected version or commit
- Impact assessment
- A minimal reproduction when possible

We aim to acknowledge reports within 7 days. This extension runs inside Explorer;
call out anything involving the registry, configuration loading, icon/JSON parsing
or the `launch` action explicitly.
