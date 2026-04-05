---
name: 'edit-grammar-rules'
description: 'Use this skill when you need to edit, add or remove a tree-sitter grammar rule'
---
We are making a programming language. Your goal is to add or remove a new grammar rule.

We use the tree-sitter library to define the grammar and do the parsing.

## Step-by-step

You need to:
1. Add a new test file to `Source/Parser/tree-sitter-hlang/test/corpus` or edit an existing one is appropriate. If you are not sure, use #tool:vscode/askQuestions.
2. Edit the grammar file `Source/Parser/tree-sitter-hlang/grammar.js` to add the new rule. Use the #tool:vscode/askQuestions to ask any questions about this.
3. Generate the parser
4. Run the tree-sitter tests to confirm that everything is working as intended. If any test fails, edit the grammar again and so-on.

## Test file template

When creating a new test file use the following template. Replace `<test-name>` and `<test-content>` appropriately. Note that you need to leave one empty line after `---` so that tree-sitter can then fill it when using `npm run test_tree_sitter_update`.

```
=========================
<test-name>
=========================

module <test-name>;

<test-content>

---

```

## Generate the parser

After making changes to grammar.js, you need to generate the parser:

```
cd Source/Parser/tree-sitter-hlang
npm run generate
```

## Running the tests

After generating the parser, you need to run the tests and update the results after `---`. This is done automatically using the following command:

```
cd Source/Parser/tree-sitter-hlang
npm run test_tree_sitter_update
````

At this point, you should check if the tests failed which can happen if any ERROR or MISSING nodes are found in the tests.
If everything is green, then you should confirm that the tests that were updated are correct and that there weren't any unintended changes.
If everything went well, then you are done.
