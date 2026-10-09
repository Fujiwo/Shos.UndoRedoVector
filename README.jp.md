# [C++] Shos.UndoRedoVector

[English](README.md) | [日本語](README.jp.md)

**アンドゥ / リドゥ** ができる vector を提供する、ヘッダーオンリーの C++ ライブラリーです。

要素の変更 (`push_back`、`insert`、`erase`、`update`、`clear`) はすべて自動的に記録されるため、
`undo()` と `redo()` で履歴を前後に移動できます。
エディターや作図ツールなど、項目のリストに対してアンドゥ履歴が必要なアプリケーションに便利です。

## 特長

- **ヘッダーオンリー**: `undo_redo_vector.h` をインクルードするだけで使えます。ビルドや依存ライブラリーは不要です。
- **自動的な履歴**: すべての変更が 1 つのアンドゥ単位になります。
- **トランザクション**: 複数の変更を 1 つのアンドゥ単位にまとめられます (入れ子も可能)。
- **所有権のサポート**: `undo_redo_pointer_vector<T>` は `T*` の要素を所有し、適切なタイミングで delete します。任意の後始末関数を指定することもできます。
- **コンテナーの選択**: 既定は `std::vector` ですが、`std::list` も使えます。
- **安全**: 変更処理が例外 (メモリー不足など) を送出した場合でも、コレクションと履歴は変更前の状態のまま保たれます。

## 動作要件

- C++17 以降 (`std::optional` を使用しているため)
- 標準ライブラリーのみを使用しています。標準に準拠したコンパイラー (MSVC、GCC、Clang) で動作します。

## 導入方法

[`undo_redo_vector.h`](undo_redo_vector.h) をプロジェクトにコピーして、インクルードしてください。

```cpp
#include "undo_redo_vector.h"
```

すべて `shos` 名前空間に含まれています。

## クイックスタート

```cpp
#include <iostream>
#include "undo_redo_vector.h"

int main()
{
    shos::undo_redo_vector<int> values;

    values.push_back(100);      // [100]
    values.push_back(200);      // [100, 200]

    values.undo();              // [100]
    values.redo();              // [100, 200]

    for (int value : values)
        std::cout << value << ' ';   // 100 200
}
```

## 使い方

### 要素の変更

すべての変更を記録するため、要素は次のメンバー関数でのみ変更できます。
位置は `std::vector` と同様にイテレーターで指定します。

```cpp
shos::undo_redo_vector<int> values;

values.push_back(100);                   // [100]
values.push_back(300);                   // [100, 300]
values.insert(values.begin() + 1, 200);  // [100, 200, 300]   指定位置の前に挿入
values.update(values.begin(), 1000);     // [1000, 200, 300]  指定位置の要素を置き換え
values.erase(values.begin() + 2);        // [1000, 200]       指定位置の要素を削除
values.clear();                          // []                全削除 (1 回のアンドゥで元に戻る)
```

> `operator[]` や `begin()`、`end()` などは `const` の参照 / イテレーターしか返さないため、
> `values[0] = 1000;` のような代入はできません。代わりに `update()` を使ってください。

### 要素の参照

通常の読み取り用の関数が使えます。

```cpp
int first  = values[0];
int second = values.at(1);       // 範囲外なら std::out_of_range を送出
int head   = values.front();
int tail   = values.back();
size_t n   = values.size();
bool none  = values.empty();

for (int value : values)          // 範囲 for 文も使える
    std::cout << value << '\n';
```

### アンドゥとリドゥ

```cpp
shos::undo_redo_vector<int> values;
values.push_back(100);                   // [100]
values.push_back(300);                   // [100, 300]
values.insert(values.begin() + 1, 200);  // [100, 200, 300]
values.update(values.begin(), 1000);     // [1000, 200, 300]
values.erase(values.begin() + 2);        // [1000, 200]

values.undo();                           // [1000, 200, 300]  erase が取り消される
values.undo();                           // [100, 200, 300]   update が取り消される
values.redo();                           // [1000, 200, 300]  update がやり直される

if (values.can_undo()) { /* ... */ }     // アンドゥできるものがあれば true
if (values.can_redo()) { /* ... */ }     // リドゥできるものがあれば true
```

`undo()` / `redo()` は、成功すると `true` を返します。アンドゥ / リドゥできるものがない場合は何もせず `false` を返します。

一般的なアプリケーションと同様に、**`undo()` の後に変更を加えるとリドゥ履歴は破棄されます**。

```cpp
values.undo();          // [100, 200, 300]
values.push_back(400);  // [100, 200, 300, 400]
values.can_redo();      // false
```

### 複数の変更を 1 回のアンドゥにまとめる (トランザクション)

`transaction` オブジェクトを作成します。このオブジェクトが生存している間に行った変更はすべて 1 つのアンドゥ単位にまとめられ、
オブジェクトがスコープを抜けた時点で確定します。

```cpp
{
    shos::undo_redo_vector<int>::transaction transaction(values);
    values.push_back(500);
    values.push_back(600);
}   // ここで 1 つのアンドゥ単位として確定

values.undo();   // 500 と 600 の両方が取り消される
```

- トランザクションは入れ子にできます。アンドゥ単位を作るのは最も外側のトランザクションだけです。
- 変更を 1 つも行わなかったトランザクションは、アンドゥ単位を作りません。
- トランザクション中は `undo()` / `redo()` は何もせず `false` を返し、`can_undo()` / `can_redo()` も `false` を返します。トランザクション中かどうかは `in_transaction()` で確認できます。
- `clear()` は内部でトランザクションを使っているため、1 回のアンドゥで元に戻せます。

### 履歴の消去 (`reset`)

`reset()` は、すべての要素**と**アンドゥ / リドゥ履歴を削除します。この操作は元に戻せません。

```cpp
values.reset();   // size() == 0, can_undo() == false, can_redo() == false
```

トランザクション中に `reset()` を呼ぶと `std::logic_error` を送出します。

### ポインターの所有 (`undo_redo_pointer_vector`)

要素が `new` で確保したオブジェクトへのポインターである場合は、`undo_redo_pointer_vector<T>` を使います。
このクラスはオブジェクトを所有し、**アンドゥ / リドゥで到達できなくなった時点で**、それぞれをちょうど 1 回だけ `delete` します。

```cpp
struct Shape  { virtual ~Shape() = default; };
struct Circle : Shape {};

shos::undo_redo_pointer_vector<Shape> shapes;

shapes.push_back(new Circle());
shapes.erase(shapes.begin());   // Circle はまだ delete されない (アンドゥ用に保持される)
shapes.undo();                  // 同じ Circle が vector に戻る
// Circle を保持している履歴が破棄されたとき、または shapes が reset / 破棄されたときに delete される
```

ポインターを自分で `delete` しないでください。

### 独自の後始末関数

その他の種類のリソース (ハンドル、ファイルなど) には、コンストラクターに後始末関数を渡します。
この関数は、要素が不要になったとき、つまり次のいずれかのときに、各要素についてちょうど 1 回だけ呼ばれます。

- その要素を保持しているアンドゥ / リドゥの単位が破棄されたとき
- コレクションが `reset()` または破棄されたとき

```cpp
shos::undo_redo_vector<Handle> handles([](Handle& handle) { close_handle(handle); });
```

`update()` で置き換えられた古い要素もアンドゥ用に保持され、後で同じように後始末されます。

### 別のコンテナーを使う (`undo_redo_collection`)

`undo_redo_vector<T>` は `undo_redo_collection<T, std::vector<T>>` の別名です。
`std::list` など、ほかのシーケンスコンテナーを指定することもできます。

```cpp
#include <list>

shos::undo_redo_collection<int, std::list<int>> values;
values.push_back(1);
values.push_back(2);
values.erase(std::next(values.begin()));   // list のイテレーターはランダムアクセスできない
values.undo();                             // [1, 2]
```

同様に、`undo_redo_pointer_collection<T, TCollection>` は `undo_redo_pointer_vector<T>` の一般形です。

## API リファレンス

### 型

| 型 | 説明 |
| --- | --- |
| `undo_redo_vector<T>` | アンドゥ / リドゥ付きの vector。`undo_redo_collection<T>` の別名 |
| `undo_redo_pointer_vector<T>` | 指す先のオブジェクトを所有して delete する `T*` の vector。`undo_redo_pointer_collection<T>` の別名 |
| `undo_redo_collection<T, TCollection = std::vector<T>>` | 内部のコンテナーを指定できる一般形 |
| `undo_redo_pointer_collection<T, TCollection = std::vector<T*>>` | ポインター版の一般形 |
| `undo_redo_collection<...>::transaction` | 変更を 1 つのアンドゥ単位にまとめる RAII オブジェクト |

### メンバー関数

| 関数 | 説明 |
| --- | --- |
| `undo_redo_collection()` | 空のコレクションを作成 |
| `undo_redo_collection(clean_up_function)` | 後始末関数 `void(T&)` を指定して空のコレクションを作成 |
| `push_back(element)` | 末尾に要素を追加 |
| `insert(position, element)` | `position` の前に要素を挿入 |
| `erase(position)` | `position` の要素を削除 |
| `update(position, element)` | `position` の要素を置き換え |
| `clear()` | すべての要素を削除 (1 つのアンドゥ単位) |
| `reset()` | すべての要素と履歴を削除 (元に戻せない) |
| `undo()` / `redo()` | 1 単位アンドゥ / リドゥ。できない場合は `false` を返す |
| `can_undo()` / `can_redo()` | `undo()` / `redo()` が可能かどうか |
| `in_transaction()` | トランザクション中かどうか |
| `operator[](index)` / `at(index)` | 要素の参照 (`const`)。`at` は範囲をチェックする |
| `front()` / `back()` | 先頭 / 末尾の要素 (`const`) |
| `size()` / `empty()` | 要素数 / 空かどうか |
| `begin()` / `end()` / `cbegin()` / `cend()` | `const_iterator` |

## 注意事項

- **読み取り専用のアクセス**: `begin()` / `end()` は `const_iterator` を、`operator[]` は `const` 参照を返します。要素を変更するには `update()` を使ってください。
- **コピー不可**です。ムーブは可能ですが、トランザクション中にムーブしてはいけません。
- **例外安全性**: `push_back` / `insert` / `erase` / `update` が例外を送出した場合、コレクションと履歴は変更前のまま保たれます。
- **スレッドセーフではありません**。複数のスレッドからアクセスする場合は、外部で同期してください。

## リポジトリーの構成

| パス | 説明 |
| --- | --- |
| [`undo_redo_vector.h`](undo_redo_vector.h) | ライブラリー本体 (必要なのはこのファイルだけ) |
| `Shos.UndoRedoVector.Test/` | 単体テスト (Microsoft C++ Unit Test Framework) |
| `Shos.UndoRedoVector.MemoryLeakTest/` | メモリーリークのテスト (MSVC CRT デバッグヒープ) |
| `Shos.UndoRedoVector.sln` | テスト用の Visual Studio ソリューション |

## テストの実行

1. `Shos.UndoRedoVector.sln` を Visual Studio (2022 以降、「C++ によるデスクトップ開発」ワークロード) で開きます。
2. ソリューションをビルドします。
3. **テスト エクスプローラー**から単体テストを実行します。
4. メモリーリークを確認するには、Debug 構成で `Shos.UndoRedoVector.MemoryLeakTest` を実行します。リークがあれば出力ウィンドウに報告されます。

## 作者

Fujio Kojima: 日本のソフトウェア開発者

* Microsoft MVP for Development Tools - Visual C# (2005 年 7 月 - 2014 年 12 月)
* Microsoft MVP for .NET (2015 年 1 月 - 2015 年 10 月)
* Microsoft MVP for Visual Studio and Development Technologies (2015 年 11 月 - 2018 年 6 月)
* Microsoft MVP for Developer Technologies (2018 年 11 月 - 2025 年 6 月)
* [MVP プロフィール](https://mvp.microsoft.com/en-US/mvp/profile/4185d172-3c9a-e411-93f2-9cb65495d3c4)
* [ブログ](http://wp.shos.info)
* [Web サイト](http://www.shos.info)
* [Twitter](https://twitter.com/Fujiwo)
* [Instagram](https://www.instagram.com/fujiwo/)

## ライセンス

[MIT License](LICENSE.txt)
