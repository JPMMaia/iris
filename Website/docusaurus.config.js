// @ts-check
const { themes: prismThemes } = require('prism-react-renderer');

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'Iris Programming Language',
  tagline: 'A compiled, statically-typed systems language with first-class C interoperability',
  favicon: 'img/favicon.ico',

  url: 'https://iris-lang.github.io',
  baseUrl: '/',

  onBrokenLinks: 'throw',

  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          path: '../Documentation',
          routeBasePath: '/',
          sidebarPath: require.resolve('./sidebars.js'),
        },
        blog: false,
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: 'Iris',
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'docsSidebar',
            position: 'left',
            label: 'Documentation',
          },
          {
            href: 'https://github.com/iris-lang/iris',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Learn',
            items: [
              { label: 'Getting Started', to: '/getting-started/installation' },
              { label: 'Language Reference', to: '/language/modules' },
            ],
          },
          {
            title: 'Interop',
            items: [
              { label: 'Importing C', to: '/interop/importing-c' },
              { label: 'Exporting as C Header', to: '/interop/exporting-as-c-header' },
            ],
          },
        ]
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
        // 'iris' is not a built-in Prism language; use 'rust' as a visual stand-in
        // until a custom Iris grammar is registered.
        additionalLanguages: ['json', 'powershell', 'cmake'],
      },
    }),
};

module.exports = config;
