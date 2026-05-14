# Documentation Organization Standards

## Directory Structure

This project uses a hierarchical documentation organization to maintain clarity and consistency.

```
OAuth2-plugin-example/
├── docs/                              # Project-level documentation (if needed)
│   └── (overall project design docs)
│
├── OAuth2Backend/
│   └── docs/                          # Backend-specific documentation
│       ├── superpowers/
│       │   ├── specs/                 # Design specifications
│       │   │   └── YYYY-MM-DD-<topic>-design.md
│       │   └── plans/                 # Implementation plans
│       │       └── YYYY-MM-DD-<topic>-plan.md
│       ├── api_reference.md           # API documentation
│       ├── architecture_overview.md   # System architecture
│       ├── ci_cd_guide.md             # CI/CD practices
│       └── (other technical guides)
│
└── OAuth2Frontend/
    └── docs/                          # Frontend-specific documentation
        └── (frontend-related docs)
```

## Document Placement Guidelines

### Project-Level Documents (`docs/`)
- **When to use:** Cross-cutting concerns affecting both backend and frontend
- **Examples:** Overall project architecture, deployment guides, contributing guidelines
- **Current status:** Rarely used, most docs are component-specific

### Backend Documents (`OAuth2Backend/docs/`)
- **When to use:** Backend-specific technical documentation
- **Categories:**
  - **superpowers/specs/**: Design specifications for backend features
  - **superpowers/plans/**: Implementation plans for backend features
  - **Technical guides**: API reference, architecture, testing, etc.
- **Examples:** API design, database schemas, CI/CD workflows, security guides

### Frontend Documents (`OAuth2Frontend/docs/`)
- **When to use:** Frontend-specific documentation
- **Examples:** Component design, UI/UX guidelines, frontend deployment

## Naming Conventions

### Design Specifications
- **Format:** `YYYY-MM-DD-<topic>-design.md`
- **Example:** `2026-04-14-multiplatform-ci-design.md`
- **Location:** `OAuth2Backend/docs/superpowers/specs/`

### Implementation Plans
- **Format:** `YYYY-MM-DD-<topic>-plan.md`
- **Example:** `2026-04-14-multiplatform-ci-plan.md`
- **Location:** `OAuth2Backend/docs/superpowers/plans/`

### Technical Guides
- **Format:** `<topic>_guide.md` or `<topic>.md`
- **Examples:** `api_reference.md`, `testing_guide.md`, `security_architecture.md`
- **Location:** `OAuth2Backend/docs/`

## Decision Tree

When creating a new document, ask:

1. **Does it affect multiple components?**
   - Yes → `docs/` (project level)
   - No → Continue to next question

2. **Is it backend-specific?**
   - Yes → `OAuth2Backend/docs/`
   - No → Continue to next question

3. **Is it frontend-specific?**
   - Yes → `OAuth2Frontend/docs/`
   - No → Consider if it needs to be a separate document

## Standards Evolution

- **Created:** 2026-04-14
- **Purpose:** Resolve ambiguity about document placement
- **Maintainer:** vilas
- **Update process:** Revise this document when organizational patterns change

---

**Remember:** Consistent documentation organization improves discoverability and reduces confusion about where to find or create documentation.
