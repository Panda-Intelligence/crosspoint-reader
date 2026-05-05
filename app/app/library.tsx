import React, { useCallback, useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  FlatList,
  Image,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';
import { useIsFocused } from '@react-navigation/native';
import * as Sharing from 'expo-sharing';

import { BooksApi, RecentBook } from '../src/api/books';
import { FilesApi } from '../src/api/files';

interface BookWithCover extends RecentBook {
  coverUri: string | null;
}

export default function LibraryScreen() {
  const router = useRouter();
  const isFocused = useIsFocused();
  const [books, setBooks] = useState<BookWithCover[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  const fetchBooks = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const list = await BooksApi.recent();
      // Resolve cover URLs in parallel.
      const withCovers: BookWithCover[] = await Promise.all(
        list.map(async (b) => ({
          ...b,
          coverUri: await BooksApi.coverImageUrl(b.coverPath),
        })),
      );
      setBooks(withCovers);
    } catch (e: any) {
      setError(e?.message ?? 'Failed to load books.');
      setBooks(null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    if (isFocused) void fetchBooks();
  }, [isFocused, fetchBooks]);

  const showActions = (book: BookWithCover) => {
    Alert.alert(
      book.title || book.path.split('/').pop() || 'Book',
      book.author ? `by ${book.author}` : '',
      [
        {
          text: 'Download to phone',
          onPress: () => downloadOne(book.path),
        },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: () => confirmDelete(book),
        },
        { text: 'Cancel', style: 'cancel' },
      ],
    );
  };

  const downloadOne = async (remotePath: string) => {
    setBusy(true);
    try {
      const result = await FilesApi.download(remotePath);
      const basename = remotePath.split('/').pop() ?? 'book';
      if (await Sharing.isAvailableAsync()) {
        await Sharing.shareAsync(result.localUri, {
          dialogTitle: `Share ${basename}`,
        });
      } else {
        Alert.alert('Saved', `${basename} saved to:\n${result.localUri}`);
      }
    } catch (e: any) {
      Alert.alert('Download failed', e?.message ?? 'Could not download.');
    } finally {
      setBusy(false);
    }
  };

  const confirmDelete = (book: BookWithCover) => {
    Alert.alert('Delete book?', book.path, [
      { text: 'Cancel', style: 'cancel' },
      {
        text: 'Delete',
        style: 'destructive',
        onPress: async () => {
          setBusy(true);
          try {
            await FilesApi.remove(book.path);
            await fetchBooks();
          } catch (e: any) {
            Alert.alert('Delete failed', e?.message ?? 'Could not delete.');
          } finally {
            setBusy(false);
          }
        },
      },
    ]);
  };

  if (loading) {
    return (
      <View style={styles.center}>
        <ActivityIndicator size="large" color="#007AFF" />
        <Text style={styles.helperText}>Loading library…</Text>
      </View>
    );
  }

  if (error) {
    return (
      <View style={styles.center}>
        <Text style={styles.errorText}>{error}</Text>
        <TouchableOpacity style={styles.retryButton} onPress={fetchBooks}>
          <Text style={styles.retryButtonText}>Retry</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.linkButton} onPress={() => router.back()}>
          <Text style={styles.linkButtonText}>Back</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <TouchableOpacity onPress={() => router.back()} style={styles.backLink}>
          <Text style={styles.backLinkText}>‹ Back</Text>
        </TouchableOpacity>
        <Text style={styles.title}>Recent Books</Text>
        <TouchableOpacity onPress={fetchBooks} style={styles.refreshLink}>
          <Text style={styles.refreshLinkText}>↻</Text>
        </TouchableOpacity>
      </View>

      <FlatList
        data={books ?? []}
        keyExtractor={(b) => b.path}
        numColumns={2}
        columnWrapperStyle={styles.columnWrapper}
        contentContainerStyle={styles.grid}
        renderItem={({ item }) => (
          <BookCard book={item} onPress={() => showActions(item)} disabled={busy} />
        )}
        ListEmptyComponent={
          <View style={styles.emptyBox}>
            <Text style={styles.emptyTitle}>No recent books yet.</Text>
            <Text style={styles.emptyHint}>
              Open a book on your Crosspoint Reader to see it here.
            </Text>
          </View>
        }
      />
    </View>
  );
}

interface BookCardProps {
  book: BookWithCover;
  onPress: () => void;
  disabled: boolean;
}

function BookCard({ book, onPress, disabled }: BookCardProps) {
  const [imageError, setImageError] = useState(false);
  const fallback = !book.coverUri || imageError;

  return (
    <TouchableOpacity style={styles.card} onPress={onPress} disabled={disabled}>
      <View style={styles.coverBox}>
        {fallback ? (
          <View style={styles.coverPlaceholder}>
            <Text style={styles.coverPlaceholderText}>📖</Text>
          </View>
        ) : (
          <Image
            source={{ uri: book.coverUri! }}
            style={styles.cover}
            onError={() => setImageError(true)}
            resizeMode="cover"
          />
        )}
      </View>
      <Text style={styles.cardTitle} numberOfLines={2}>
        {book.title || book.path.split('/').pop() || 'Untitled'}
      </Text>
      {!!book.author && (
        <Text style={styles.cardAuthor} numberOfLines={1}>
          {book.author}
        </Text>
      )}
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#fff', paddingTop: 50 },
  center: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#fff',
    padding: 20,
  },
  helperText: { marginTop: 10, color: '#666', fontSize: 14 },
  errorText: {
    color: '#c44',
    fontSize: 14,
    textAlign: 'center',
    marginBottom: 12,
  },
  retryButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 8,
    marginBottom: 8,
  },
  retryButtonText: { color: '#fff', fontWeight: '600' },
  linkButton: { padding: 12 },
  linkButtonText: { color: '#007AFF', fontSize: 14 },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#eee',
  },
  backLink: { paddingVertical: 6, paddingHorizontal: 10, minWidth: 60 },
  backLinkText: { color: '#007AFF', fontSize: 16, fontWeight: '600' },
  title: { flex: 1, textAlign: 'center', fontSize: 18, fontWeight: '600' },
  refreshLink: { paddingVertical: 6, paddingHorizontal: 12, minWidth: 60, alignItems: 'flex-end' },
  refreshLinkText: { color: '#007AFF', fontSize: 22 },
  grid: { padding: 12, paddingBottom: 60 },
  columnWrapper: { gap: 12, marginBottom: 16 },
  card: { flex: 1, alignItems: 'flex-start' },
  coverBox: {
    width: '100%',
    aspectRatio: 0.66,
    borderRadius: 6,
    overflow: 'hidden',
    backgroundColor: '#f0f0f0',
    marginBottom: 8,
  },
  cover: { width: '100%', height: '100%' },
  coverPlaceholder: {
    width: '100%',
    height: '100%',
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#eee',
  },
  coverPlaceholderText: { fontSize: 48, color: '#999' },
  cardTitle: { fontSize: 14, fontWeight: '600', color: '#333' },
  cardAuthor: { fontSize: 12, color: '#888', marginTop: 2 },
  emptyBox: { padding: 40, alignItems: 'center' },
  emptyTitle: { fontSize: 16, color: '#666', marginBottom: 6 },
  emptyHint: { fontSize: 13, color: '#999', textAlign: 'center' },
});
