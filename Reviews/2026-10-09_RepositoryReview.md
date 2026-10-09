# Shos.UndoRedoVector リポジトリ レビュー

- レビュー日: 2026-10-09
- 対象コミット: `f1633d7` (main)
- 対象範囲: リポジトリ全体 (`undo_redo_vector.h`, テストプロジェクト 2 本, ソリューション/プロジェクト設定, README, リポジトリ構成)
- 検証方法: コードの精読に加え、`g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` で再現コードを実行し、指摘の多くを実際に確認した (各項目に「検証済み」と記載)。

---

## 1. 総評

`undo_redo_vector.h` は、ヘッダー 1 本で使える小さな Undo/Redo 対応コレクションです。

- `undo_step::redo()` を `undo()` のトグルで実装し、add と remove を状態ごとに入れ替える設計は簡潔で巧みです。
- `transaction` (RAII) で複数操作を 1 ステップにまとめられる API は使いやすいです。
- clean_up 関数を使ったポインタ要素の所有権管理 (履歴にしか存在しない要素も破棄時に解放する) は、基本的な経路では正しく動作します。検証では、push_back → update → erase → undo → undo → push_back の後に破棄すると、3 要素すべてが 1 回ずつ解放されました。

一方で、次のような問題があります。

- **`reset()` が clean_up なしで null 参照する**
- **コピー時に二重解放する**
- **トランザクション中の `undo()`/`reset()`/`clear()` が未定義動作・`std::terminate`・例外になる**

いずれも通常の使い方で起こり得るため、修正を推奨します。テストは主要な正常系を押さえていますが、上記の異常系は 1 件もカバーしていません。

---

## 2. 重大度: 高 (バグ・未定義動作)

### 2.1 `reset()` が clean_up 未指定時に null ポインタ参照する 【検証済み】

`undo_redo_vector.h:254-258`, `:398-402`

```cpp
void reset()
{
    clean_up_elements();   // ← clean_up の null チェックなし
    reset_undo_steps();
}

void clean_up_elements()
{
    std::for_each(this->begin(), this->end(), [&](TElement element) { (*clean_up)(element); });
    data.clear();
}
```

デストラクタでは `if (clean_up != nullptr)` で保護していますが、`reset()` にはこの保護がありません。そのため、`undo_redo_vector<int> a; a.push_back(1); a.reset();` は null 参照で未定義動作になります。UBSan では `member call on null pointer of type 'const struct clean_up_function'` が出ました。

**修正案**: `clean_up_elements()` の中で null チェックを行い、clean_up がなくても `data.clear()` は必ず実行する。

### 2.2 コピー/ムーブで二重解放・ダングリング参照になる (Rule of Three/Five 違反) 【検証済み】

`undo_redo_collection` は次のリソースを生ポインタで所有しています。

- `undo_steps` (`std::vector<undo_step*>`)
- `current_undo_step_group`
- `clean_up` (`new clean_up_function`)

それにもかかわらず、コピーコンストラクタとコピー代入を禁止していません。さらに各 `undo_step` は `TCollection& collection` を保持しており、これはコピー元の `data` を指しています。

`auto b = a;` の後にスコープを抜けると、ASan が `heap-use-after-free` を報告しました。

**修正案**: 最低限、次のように宣言する。

```cpp
undo_redo_collection(const undo_redo_collection&)            = delete;
undo_redo_collection& operator=(const undo_redo_collection&) = delete;
```

ムーブを許す場合は、`undo_step` が `TCollection&` を持つ設計を見直す必要があります (例: `undo()`/`redo()` の引数でコレクションを渡す)。`transaction` も参照を保持するので、コピーを禁止すべきです。

### 2.3 トランザクション中の `undo()`/`redo()` で範囲外アクセスする 【検証済み】

`undo_redo_vector.h:278-296`

トランザクション中でも `undo()`/`redo()` は確定済みの `undo_steps` を操作してしまいます。その結果、オープン中のグループ内ステップが前提にしているインデックスと、実際のデータが食い違います。

```cpp
a.push_back(1); a.push_back(2);
{ transaction t(a); a.erase(a.begin() + 1); a.undo(); }
a.undo(); // ← heap-buffer-overflow (ASan)
```

**修正案**: `current_undo_step_group != nullptr` のときは、`undo()`/`redo()` で例外を投げるか `false` を返す。`can_undo()`/`can_redo()` もトランザクション中は `false` を返すようにする。

### 2.4 トランザクション中の `reset()` で `std::terminate` する 【検証済み】

`undo_redo_vector.h:318-321`, `:333-336`, `:393-394`

`reset()` → `reset_undo_steps()` が `current_undo_step_group` を delete して nullptr にします。そのため、スコープ終了時に `~transaction()` → `end_transaction()` が `std::logic_error` を投げます。デストラクタは暗黙の `noexcept` なので、プロセスは `std::terminate` で異常終了しました (exit code 134)。

**修正案**: 次の 2 点を推奨。
- デストラクタからは例外を投げない (`end_transaction()` を no-throw にする。不整合は `assert` で検出する)。
- トランザクション中の `reset()` を禁止する。

### 2.5 トランザクション中に `clear()` を呼ぶと必ず例外になる 【検証済み】

`undo_redo_vector.h:247-252`, `:325-331`

`clear()` は内部で `transaction` を開始しますが、トランザクションの入れ子に対応していないため、`begin_transaction()` が `logic_error("an exception occurred")` を投げます。「複数の編集の中で全削除する」という自然な使い方ができません。

**修正案**: ネストカウンタを導入し、最外側のトランザクションだけがグループを確定するようにする。これは一般的な Undo マネージャの挙動です。

---

## 3. 重大度: 中 (設計・API・安全性)

### 3.1 非 const イテレータを公開しているため、Undo 履歴を経由しない変更ができる 【検証済み】

`begin()`/`end()` が `TCollection::iterator` を返すので、`*a.begin() = 99;` のような変更が履歴に残りません。その後の undo/redo との整合性も壊れます。

`clean_up` 付きのポインタコレクションでは、上書きされた古いポインタがリークし、新しいポインタは二重管理になるおそれがあります。

**修正案**:
- `begin()`/`end()` は `const_iterator` を返す。
- `erase`/`update` には `const_iterator` を受け取るオーバーロードを用意する。
- 範囲 for を const オブジェクトで使えるよう、`begin() const`/`end() const` を追加する。現在は `cbegin`/`cend` しかないため、`for (auto& x : const_obj)` がコンパイルできません。

### 3.2 `TCollection` テンプレート引数は実質 random-access コンテナ専用 【検証済み】

`undo_step::remove`/`update`/`undo` は `collection[index]` や `begin() + index` を使っています。そのため `std::list` を指定するとコンパイルエラーになります。`<list>` を include しているので list 対応を意図していたように見えますが、実際には対応していません。

**修正案**: 次のどちらかにする。
- `std::next(collection.begin(), index)` に統一して list にも対応する。
- `static_assert` で random-access イテレータを要求し、不要な `#include <list>` を削除する。

### 3.3 例外メッセージが情報を持たない

`"an exception occurred"` が 3 か所で使われていて、原因を区別できません。たとえば `"transaction already in progress"` や `"no transaction in progress"` のような具体的なメッセージにすべきです。

### 3.4 手動 `new`/`delete` による例外安全性の欠如

- `undo_step::add` は `collection.push_back(element)` の後で `new undo_step(...)` します。`new` が失敗すると、データは追加されたのに履歴が残りません。`remove`/`update` も同じで、データを変更した後にステップを確保しています。
- `push_to_steps` の `undo_steps.push_back(step)` が失敗すると `step` がリークします。

**修正案**:
- `std::unique_ptr<undo_step>` で管理する (`std::vector<std::unique_ptr<undo_step>>`)。手書きの `for_each(... delete ...)` も不要になります。
- ステップを先に生成し、その後にデータを変更する順序にする。

### 3.5 clean_up 関数の保持方法が冗長

`clean_up_function` クラスは `std::function` を包むだけです。それをさらに `new` してヒープに置き、生ポインタを各ステップに配っています。

**修正案**: `std::function<void(TElement)>` をメンバとして直接持ち、空かどうかを判定に使う。各ステップへはポインタではなく、`undo()`/デストラクタ呼び出し時に渡す形にするとより安全です。

### 3.6 値渡しとコピーのコスト

`push_back(TElement element)`, `update(iterator, TElement element)`, `std::function<void(TElement)>` はいずれも値渡しで、`std::move` を使っていません。`std::string` などの重い型を要素にすると、操作のたびに複数回コピーされます。

**修正案**: `element` を `std::move` で格納する。clean_up のシグネチャは `void(TElement&)` か `void(const TElement&)` にする。

### 3.7 `clear()` が O(n²)

`erase(begin())` を繰り返しているため、vector では O(n²) になり、ステップも n 個生成されます。末尾から削除すれば各 erase は O(1) です。根本的には、「全要素を一括で保持する clear ステップ」を 1 つ作るのが効率的です。

---

## 4. 重大度: 低 (保守性・スタイル)

| # | 箇所 | 内容 |
|---|------|------|
| 4.1 | `undo_redo_vector.h:86` | `switch` に `operation_type::group` の case がない (`-Wswitch` 警告)。`default:` を追加するか、group は別クラスで処理されることをコメントで明示する。 |
| 4.2 | `undo_redo_vector.h:372-386` | `undo_data()` はどこからも呼ばれていない (デッドコード)。引数も `std::vector<TElement>` 固定で、`TCollection` と一致しない。これに付随する `get_data()`, `get_element()`, `undo_step_group::cbegin/cend/begin/end` も未使用。 |
| 4.3 | `undo_redo_vector.h:49` | `get_operation_type()` に `const` が付いていない。 |
| 4.4 | `undo_redo_vector.h:45` | `hasElement` だけ camelCase で、他は snake_case。 |
| 4.5 | 全体 | `size_t` と `std::size_t` が混在している。 |
| 4.6 | `undo_redo_vector.h:104-107` | `redo()` が `undo()` を呼ぶトグル設計は分かりにくい。「操作種別を反転して状態を入れ替えるため、redo は undo と同じ処理になる」という説明コメントがあるとよい。 |
| 4.7 | API | `empty()`, `at()`, `front()`/`back()`, `insert()` など、vector として期待される基本 API がない。 |
| 4.8 | `undo_redo_vector.h:15-27` | 内部クラス `undo_step`/`clean_up_function` が `private` セクションに置かれていることを明示していない (class のデフォルト private に依存している)。可読性のため `private:` を明記するとよい。 |

---

## 5. テスト (`Shos.UndoRedoVector.Test`)

### 5.1 問題点

1. **`check_vector` のループが 1 回も実行されない** (`Shos.UndoRedoVector.Test.cpp:19`)
   条件が `iterator != array.begin()` なので、初回から false になります。`array.end()` の誤りと思われます。
2. **`Assert::AreEqual` の引数順が逆**
   MSTest C++ の規約は `AreEqual(expected, actual)` ですが、全体を通して `(actual, expected)` になっています。失敗時のメッセージで expected と actual が逆に表示されます。
3. **テスト名の誤字**: `erace` → `erase`。
4. **`clean_up` テストはリークを内包している**
   `undo_redo_vector<foo*>` を clean_up なしで使い、最後に現在の要素だけを delete しています。そのため、履歴 (remove/update ステップ) に残ったポインタはリークします。検証では、同じパターンで LeakSanitizer がリークを検出しました。テストの意図 (「clean_up がないと何が起きるか」なのか「手動解放の例」なのか) を明確にすべきです。
5. **リーク検出やカウント検証がない**
   `pointer_vector` テストは解放されたかどうかを何も検証していません。カウンタ付きの clean_up ラムダを使い、解放回数を Assert すると効果的です。
6. **未カバーのシナリオ**
   - `reset()` (2.1 のバグはテストがあれば検出できた)
   - 空のトランザクション
   - トランザクション中の undo/redo/reset/clear (2.3〜2.5)
   - redo 履歴が切り捨てられたときの clean_up 呼び出し
   - `update` の redo
   - 範囲外イテレータ

### 5.2 不要・壊れたファイル

- **`Shos.MiniCadSample.Test.cpp`**: 存在しない `../Shos.MiniCadSample/undo_redo_vector.h` を include しており、`.vcxproj` にも含まれていません。内容は `Shos.UndoRedoVector.Test.cpp` とほぼ同一の重複です。削除を推奨します。
- **`Shos.UndoRedoVector.Test.BAK`**: 古いバックアップファイルで、`AreEqual<size_t>` になっている旧版です。バージョン管理下にあるので不要です。削除し、`.gitignore` に `*.BAK` を追加することを推奨します。

---

## 6. メモリリークテスト (`Shos.UndoRedoVector.MemoryLeakTest`)

1. **`undo_redo_pointer_vector_memory_leak_test()` が `main()` から呼ばれていない** (`Shos.UndoRedoVector.MemoryLeakTest.cpp:59-63`)。そのため `undo_redo_pointer_vector` はリーク検査の対象になっていません。
2. **`#define new DEBUG_NEW` を標準ヘッダより前に定義している**
   `MemoryLeakTest.h` を `undo_redo_vector.h` (`<vector>` などを含む) より先に include しています。キーワード `new` をマクロで再定義した状態で標準ヘッダを取り込むことは標準上許されていません ([macro.names])。MSVC の STL は `push_macro("new")` で自衛しているので動きますが、移植性はありません。
   **修正案**: 標準ヘッダ/対象ヘッダを先に include し、`MemoryLeakTest.h` は最後に include する。
3. **成否が自動判定されない**
   リーク報告はデバッガの出力ウィンドウに出るだけで、CI などで自動判定できません。`_CrtDumpMemoryLeaks()` の戻り値を終了コードに反映する方法があります。また、非 MSVC 環境向けに ASan/LSan で同じテストを回せるようにする (CMake 化) と効果的です。
4. `foo::operator int()` に `const` が付いていない (2 つのテストファイルで同じ)。

---

## 7. ビルド設定・リポジトリ構成

1. **言語標準が構成ごとにばらばら**
   直近のコミット `4d170be` のメッセージは "Update to C++ 17" ですが、実際の設定は次のとおりです。
   - x64 (Debug/Release) は `stdcpp20`
   - Win32 (Debug/Release) は未指定 (MSVC 既定の C++14)

   コミットメッセージとも食い違っています。どれか 1 つ (例: `stdcpp17`) に統一することを推奨します。
2. **警告レベルが構成ごとにばらばら**: Level3 と Level4 が混在しています。全構成で `/W4` に統一し、可能なら `/WX` (警告をエラー扱い) にすることを推奨します。
3. **MSVC 以外のビルド手段がない**
   ヘッダーオンリーのライブラリなので、`CMakeLists.txt` と移植性のあるテストフレームワーク (Catch2, GoogleTest など) を用意すると、Linux/macOS や GitHub Actions でも検証できます。現状はテストが `CppUnitTest.h` (Visual Studio 専用) に依存しています。
4. **CI がない**: GitHub Actions で Windows (MSBuild + vstest) を回すだけでも回帰を防げます。

---

## 8. ドキュメント (README.md)

1. **リストのインデントでタブとスペースが混在している**ため、Markdown の入れ子が崩れます。たとえば `undo()` と `redo()` の階層が揃っていません。
2. **使い方のサンプルコードがない**。最低限、次の 3 点の例が欲しいところです。
   - `push_back`/`erase`/`update`/`undo`/`redo`
   - `transaction` によるグループ化
   - `undo_redo_pointer_vector` (または clean_up 関数) による所有権管理
3. **制約が書かれていない**
   - トランザクションは入れ子にできない
   - トランザクション中に undo してはいけない
   - コピーできない (修正後)
   - イテレータ経由で直接書き換えてはいけない
   - 必要な C++ 標準のバージョン
4. 「Development Environment」の項目が `Language: C++` だけです。対応コンパイラや Visual Studio のバージョンを追記するとよいです。

---

## 9. 推奨対応の優先順位

| 優先度 | 対応 | 該当 |
|--------|------|------|
| 1 | `reset()` の null チェック | 2.1 |
| 2 | コピー禁止 (`= delete`) | 2.2 |
| 3 | トランザクション中の undo/redo/reset を禁止し、`end_transaction` を no-throw にする | 2.3, 2.4 |
| 4 | 上記の回帰テストを追加し、`check_vector` のループ条件を修正し、AreEqual の引数順を直す | 5.1 |
| 5 | 不要ファイル (`.BAK`, `Shos.MiniCadSample.Test.cpp`) を削除し、`main()` で未呼び出しのリークテストを呼ぶ | 5.2, 6.1 |
| 6 | 公開イテレータを const にする | 3.1 |
| 7 | 入れ子トランザクションに対応する | 2.5 |
| 8 | `unique_ptr` 化、ムーブ対応、デッドコードの削除 | 3.4〜3.6, 4.2 |
| 9 | ビルド設定の統一、CMake/CI の整備、README の拡充 | 7, 8 |

---

## 付録: 検証に使った再現コード (抜粋)

```cpp
// 2.1  reset() の null 参照 → UBSan: member call on null pointer
undo_redo_vector<int> a; a.push_back(1); a.reset();

// 2.2  コピー → ASan: heap-use-after-free
undo_redo_vector<int> a; a.push_back(1); { auto b = a; }

// 2.3  トランザクション中の undo → ASan: heap-buffer-overflow
undo_redo_vector<int> a; a.push_back(1); a.push_back(2);
{ undo_redo_vector<int>::transaction t(a); a.erase(a.begin() + 1); a.undo(); }
a.undo();

// 2.4  トランザクション中の reset → terminate (logic_error がデストラクタから送出される)
undo_redo_vector<int*> a([](int* p) { delete p; });
a.push_back(new int(1));
{ undo_redo_vector<int*>::transaction t(a); a.push_back(new int(2)); a.reset(); }

// 2.5  トランザクション中の clear → logic_error("an exception occurred")
undo_redo_vector<int> a; a.push_back(1);
{ undo_redo_vector<int>::transaction t(a); a.clear(); }

// 3.1  イテレータ経由の書き換えが履歴に残らない
undo_redo_vector<int> a; a.push_back(1); *a.begin() = 99; a.undo(); a.redo(); // a[0] == 99

// 3.2  std::list を指定するとコンパイルエラー
undo_redo_collection<int, std::list<int>> l; l.push_back(1); l.erase(l.begin());
```

---

## 10. 対応状況 (2026-10-09 追記)

### 10.1 指摘ごとの対応

| 指摘 | 状態 | 対応内容 |
|------|------|----------|
| 2.1 `reset()` の null 参照 | 修正済み | clean_up は `std::function` を直接保持し、空のときは呼ばない。 |
| 2.2 コピーで二重解放 | 修正済み | コピーを `= delete` にした。ステップがコレクションへの参照を持たない設計に変え、ムーブ構築とムーブ代入に対応した。 |
| 2.3 トランザクション中の undo/redo | 修正済み | トランザクション中は `can_undo()`/`can_redo()` が false を返し、`undo()`/`redo()` は何もせず false を返す。`in_transaction()` を追加した。 |
| 2.4 トランザクション中の reset で terminate | 修正済み | `reset()` はトランザクション中に `logic_error` を投げる。`end_transaction()` は `noexcept` にし、内部でメモリを確保しない (必要な領域はトランザクション開始時に確保する)。 |
| 2.5 トランザクションの入れ子 | 修正済み | 入れ子の深さを数え、最外側のトランザクションだけが 1 つの undo ステップを確定する。 |
| 3.1 非 const イテレータの公開 | 修正済み | `begin()`/`end()` は `const_iterator` を返す (`iterator` は `const_iterator` の別名)。const オブジェクトでも範囲 for が使える。 |
| 3.2 `std::list` 非対応 | 修正済み | `std::next` に統一し、`std::list` でも動作する (テストあり)。 |
| 3.3 例外メッセージ | 修正済み | 原因が分かる具体的なメッセージにした。 |
| 3.4 例外安全性 | 修正済み | `std::unique_ptr` で管理する。記録領域を先に確保してからデータを変更する。 |
| 3.5 clean_up の保持方法 | 修正済み | ラッパークラスと `new` を廃止した。 |
| 3.6 コピーコスト | 修正済み | 要素は `std::move` で受け渡す。clean_up のシグネチャは `void(TElement&)` にした。ムーブ専用の型 (`std::unique_ptr`) も要素にできる (テストあり)。 |
| 3.7 `clear()` が O(n²) | 一部対応 | 末尾から削除するので O(n) になった。ステップは要素数ぶん作るまま (1 つのトランザクションにまとめられる)。 |
| 4.1〜4.6, 4.8 | 修正済み | switch 文は廃止した (操作ごとのクラスに分割)。デッドコードを削除し、命名と `std::size_t` を統一し、設計のコメントを追加し、`private:` を明記した。 |
| 4.7 基本 API の不足 | 修正済み | `empty()`, `at()`, `front()`, `back()`, `insert()` を追加した。 |
| 5.1 テスト | 修正済み | AreEqual の引数順を (expected, actual) に直し、誤字を修正した。`check_vector`/`check_list` は標準ライブラリのテストでライブラリの検証になっていなかったため削除した。インスタンス数と clean_up 回数を検証するテストと、2.x の回帰テストを追加した (計 26 件)。 |
| 5.2 不要ファイル | 修正済み | `Shos.UndoRedoVector.Test.BAK` と `Shos.MiniCadSample.Test.cpp` を削除し、`.gitignore` に `*.BAK` を追加した。 |
| 6.1〜6.4 リークテスト | 修正済み | ポインタ版のテストも呼ぶようにした。`MemoryLeakTest.h` を最後に include し、使い方のコメントも更新した。Debug ビルドではリークがあると終了コード 1 を返す。`operator int() const` に直した。 |
| 7.1, 7.2 ビルド設定 | 修正済み | 全構成 (Debug/Release × Win32/x64)・両プロジェクトで `stdcpp17` と `Level4` に統一した。 |
| 7.3, 7.4 CMake / CI | 未対応 | 今後の課題。 |
| 8 README | 修正済み | 使い方のサンプル、トランザクション、所有権、制約を記載した。サンプルはコンパイルして実行し、コメントどおりの結果になることを確認済み。 |

### 10.2 互換性に影響する変更

1. `begin()`/`end()` が `const_iterator` を返すようになった。イテレータ経由で要素を書き換えていたコードはコンパイルエラーになる (意図した変更)。
2. C++17 以上が必須になった (`std::optional` を使うため)。
3. clean_up 関数の型が `std::function<void(TElement&)>` になった。値渡しのラムダ (`[](foo* p) { delete p; }`) はそのまま使える。
4. トランザクション中の `undo()`/`redo()` は何もせず false を返す。`reset()` は例外を投げる。

### 10.3 検証

MSVC と CppUnitTest は手元の環境にないため、次の方法で検証した。

- g++ (C++17/C++20) と ASan/UBSan で、テストコードをそのままビルドして実行した。CppUnitTest の最小限の代替ヘッダーを作業ディレクトリに用意し、それを使った。
  - 結果: 26 件すべて成功。`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` で警告なし。サニタイザのエラーなし。
- メモリリークテストを LeakSanitizer で実行した。
  - 結果: リークなし。
- Visual Studio でのビルドとテスト実行は未確認。

### 10.4 その他

- `Shos.UndoRedoVector.Test.vcxproj` の `RootNamespace` が `ShosMiniCadSampleTest` のまま残っている (動作に影響はない)。
