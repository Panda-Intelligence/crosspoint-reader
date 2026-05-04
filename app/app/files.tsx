import React, { useCallback, useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  FlatList,
  Modal,
  StyleSheet,
  Text,
  TextInput,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';

import { DeviceFileEntry, FilesApi } from '../src/api/files';

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GB`;
}

export default function FilesScreen() {
  const router = useRouter();
  const [path, setPath] = useState<string>('/');
  const [entries, setEntries] = useState<DeviceFileEntry[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  // mkdir modal
  const [mkdirOpen, setMkdirOpen] = useState(false);
  const [mkdirName, setMkdirName] = useState('');

  const fetchList = useCallback(async (p: string) => {
    setLoading(true);
    setError(null);
    try {
      setEntries(await FilesApi.list(p));
    } catch (e: any) {
      setError(e?.message ?? 'Failed to load files.');
      setEntries(null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void fetchList(path);
  }, [path, fetchList]);

  const open = (entry: DeviceFileEntry) => {
    if (entry.isDirectory) {
      setPath(FilesApi.joinPath(path, entry.name));
      return;
    }
    Alert.alert(
      entry.name,
      `${formatSize(entry.size)}${entry.isEpub ? ' · EPUB' : ''}`,
      [
        {
          text: 'Download to phone',
          onPress: () => downloadOne(FilesApi.joinPath(path, entry.name)),
        },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: () => confirmDelete(entry),
        },
        { text: 'Cancel', style: 'cancel' },
      ],
    );
  };

  const downloadOne = async (remotePath: string) => {
    setBusy(true);
    try {
      const result = await FilesApi.download(remotePath);
      Alert.alert(
        'Saved',
        `${remotePath.split('/').pop()} (${formatSize(result.size)}) saved to:\n${result.localUri}`,
      );
    } catch (e: any) {
      Alert.alert('Download failed', e?.message ?? 'Could not download.');
    } finally {
      setBusy(false);
    }
  };

  const confirmDelete = (entry: DeviceFileEntry) => {
    const fullPath = FilesApi.joinPath(path, entry.name);
    Alert.alert(
      'Delete?',
      `Permanently delete "${entry.name}" from the device?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: async () => {
            setBusy(true);
            try {
              await FilesApi.remove(fullPath);
              await fetchList(path);
            } catch (e: any) {
              Alert.alert('Delete failed', e?.message ?? 'Could not delete.');
            } finally {
              setBusy(false);
            }
          },
        },
      ],
    );
  };

  const goUp = () => {
    if (path === '/') {
      router.back();
      return;
    }
    setPath(FilesApi.parentOf(path));
  };

  const submitMkdir = async () => {
    const name = mkdirName.trim();
    if (!name) return;
    if (name.includes('/') || name.includes('\\')) {
      Alert.alert('Invalid name', 'Folder name cannot contain slashes.');
      return;
    }
    setBusy(true);
    try {
      await FilesApi.mkdir(path, name);
      setMkdirOpen(false);
      setMkdirName('');
      await fetchList(path);
    } catch (e: any) {
      Alert.alert('Create folder failed', e?.message ?? 'Could not create folder.');
    } finally {
      setBusy(false);
    }
  };

  return (
    <View style={styles.container}>
      {/* Breadcrumb / path bar */}
      <View style={styles.pathBar}>
        <TouchableOpacity onPress={goUp} style={styles.upButton} disabled={busy}>
          <Text style={styles.upButtonText}>{path === '/' ? '‹ Back' : '‹ Up'}</Text>
        </TouchableOpacity>
        <Text style={styles.pathText} numberOfLines={1} ellipsizeMode="middle">
          {path}
        </Text>
        <TouchableOpacity
          onPress={() => fetchList(path)}
          style={styles.refreshButton}
          disabled={busy || loading}>
          <Text style={styles.refreshButtonText}>↻</Text>
        </TouchableOpacity>
      </View>

      {loading ? (
        <View style={styles.center}>
          <ActivityIndicator size="large" color="#007AFF" />
        </View>
      ) : error ? (
        <View style={styles.center}>
          <Text style={styles.errorText}>{error}</Text>
          <TouchableOpacity
            style={styles.retryButton}
            onPress={() => fetchList(path)}>
            <Text style={styles.retryButtonText}>Retry</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <FlatList
          data={entries ?? []}
          keyExtractor={(e) => `${e.isDirectory ? 'd' : 'f'}:${e.name}`}
          ItemSeparatorComponent={() => <View style={styles.sep} />}
          renderItem={({ item }) => (
            <TouchableOpacity
              style={styles.row}
              onPress={() => open(item)}
              disabled={busy}>
              <Text style={styles.icon}>
                {item.isDirectory ? '📁' : item.isEpub ? '📖' : '📄'}
              </Text>
              <View style={styles.rowMain}>
                <Text style={styles.rowName} numberOfLines={1}>
                  {item.name}
                </Text>
                <Text style={styles.rowMeta}>
                  {item.isDirectory ? 'Folder' : formatSize(item.size)}
                </Text>
              </View>
              <Text style={styles.chev}>›</Text>
            </TouchableOpacity>
          )}
          ListEmptyComponent={
            <View style={styles.center}>
              <Text style={styles.emptyText}>(empty folder)</Text>
            </View>
          }
        />
      )}

      <View style={styles.actions}>
        <TouchableOpacity
          style={[styles.button, busy && styles.buttonDisabled]}
          onPress={() => setMkdirOpen(true)}
          disabled={busy}>
          <Text style={styles.buttonText}>+ New Folder</Text>
        </TouchableOpacity>
      </View>

      <Modal
        animationType="fade"
        transparent
        visible={mkdirOpen}
        onRequestClose={() => setMkdirOpen(false)}>
        <View style={styles.modalBackdrop}>
          <View style={styles.modalCard}>
            <Text style={styles.modalTitle}>New Folder</Text>
            <Text style={styles.modalHint}>under {path}</Text>
            <TextInput
              style={styles.modalInput}
              value={mkdirName}
              onChangeText={setMkdirName}
              placeholder="Folder name"
              placeholderTextColor="#aaa"
              autoCapitalize="none"
              autoCorrect={false}
              autoFocus
            />
            <View style={styles.modalActions}>
              <TouchableOpacity
                style={styles.modalCancel}
                onPress={() => {
                  setMkdirOpen(false);
                  setMkdirName('');
                }}
                disabled={busy}>
                <Text style={styles.modalCancelText}>Cancel</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={[styles.modalSubmit, busy && styles.buttonDisabled]}
                onPress={submitMkdir}
                disabled={busy || !mkdirName.trim()}>
                {busy ? (
                  <ActivityIndicator color="#fff" />
                ) : (
                  <Text style={styles.modalSubmitText}>Create</Text>
                )}
              </TouchableOpacity>
            </View>
          </View>
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#fff', paddingTop: 50 },
  pathBar: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#ddd',
    gap: 8,
  },
  upButton: { paddingVertical: 6, paddingHorizontal: 10 },
  upButtonText: { color: '#007AFF', fontSize: 16, fontWeight: '600' },
  pathText: { flex: 1, fontSize: 13, color: '#444', fontFamily: 'monospace' },
  refreshButton: { paddingVertical: 6, paddingHorizontal: 12 },
  refreshButtonText: { color: '#007AFF', fontSize: 22 },
  center: { flex: 1, justifyContent: 'center', alignItems: 'center', padding: 20 },
  errorText: { color: '#c44', fontSize: 14, textAlign: 'center', marginBottom: 12 },
  retryButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 8,
  },
  retryButtonText: { color: '#fff', fontWeight: '600' },
  emptyText: { color: '#888', fontSize: 14 },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 10,
    paddingHorizontal: 12,
    gap: 12,
  },
  icon: { fontSize: 22 },
  rowMain: { flex: 1 },
  rowName: { fontSize: 15, color: '#333', fontWeight: '500' },
  rowMeta: { fontSize: 12, color: '#888', marginTop: 2 },
  chev: { fontSize: 22, color: '#bbb' },
  sep: { height: StyleSheet.hairlineWidth, backgroundColor: '#eee', marginLeft: 56 },
  actions: { padding: 12, borderTopWidth: StyleSheet.hairlineWidth, borderTopColor: '#ddd' },
  button: {
    backgroundColor: '#007AFF',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
  },
  buttonDisabled: { opacity: 0.6 },
  buttonText: { color: '#fff', fontWeight: '600', fontSize: 15 },
  modalBackdrop: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.4)',
    justifyContent: 'center',
    alignItems: 'center',
    padding: 20,
  },
  modalCard: {
    backgroundColor: '#fff',
    borderRadius: 12,
    padding: 20,
    width: '100%',
    maxWidth: 360,
  },
  modalTitle: { fontSize: 18, fontWeight: '600', marginBottom: 4 },
  modalHint: { fontSize: 12, color: '#888', marginBottom: 12 },
  modalInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    paddingVertical: 8,
    paddingHorizontal: 10,
    fontSize: 15,
    color: '#333',
    marginBottom: 16,
  },
  modalActions: { flexDirection: 'row', gap: 12 },
  modalCancel: {
    flex: 1,
    paddingVertical: 10,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#ccc',
    alignItems: 'center',
  },
  modalCancelText: { color: '#444', fontWeight: '600' },
  modalSubmit: {
    flex: 1,
    backgroundColor: '#007AFF',
    paddingVertical: 10,
    borderRadius: 8,
    alignItems: 'center',
  },
  modalSubmitText: { color: '#fff', fontWeight: '600' },
});
