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
import { useIsFocused } from '@react-navigation/native';
import * as Sharing from 'expo-sharing';

import { DeviceFileEntry, FilesApi } from '../src/api/files';

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GB`;
}

export default function FilesScreen() {
  const router = useRouter();
  const isFocused = useIsFocused();
  const [path, setPath] = useState<string>('/');
  const [entries, setEntries] = useState<DeviceFileEntry[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  // mkdir modal
  const [mkdirOpen, setMkdirOpen] = useState(false);
  const [mkdirName, setMkdirName] = useState('');

  // Rename modal — `target` holds the path being renamed.
  const [renameTarget, setRenameTarget] = useState<DeviceFileEntry | null>(null);
  const [renameName, setRenameName] = useState('');

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
    if (isFocused) {
      void fetchList(path);
    }
  }, [path, isFocused, fetchList]);

  const open = (entry: DeviceFileEntry) => {
    if (entry.isDirectory) {
      // Directories: tap navigates in. Long-press would be the natural
      // place for the action sheet, but Alert.alert + onLongPress isn't
      // ergonomic on Android, so we surface a nav-vs-actions choice for
      // folders too. Tap → navigate; long-press → actions.
      setPath(FilesApi.joinPath(path, entry.name));
      return;
    }
    showActions(entry);
  };

  const showActions = (entry: DeviceFileEntry) => {
    const fullPath = FilesApi.joinPath(path, entry.name);
    Alert.alert(
      entry.name,
      `${entry.isDirectory ? 'Folder' : `${formatSize(entry.size)}${entry.isEpub ? ' · EPUB' : ''}`}`,
      [
        ...(entry.isDirectory
          ? []
          : [
              {
                text: 'Download to phone',
                onPress: () => downloadOne(fullPath),
              } as const,
            ]),
        {
          text: 'Rename',
          onPress: () => {
            setRenameTarget(entry);
            setRenameName(entry.name);
          },
        } as const,
        {
          text: 'Move…',
          onPress: () =>
            router.push({ pathname: '/move-picker', params: { source: fullPath } }),
        } as const,
        {
          text: 'Delete',
          style: 'destructive' as const,
          onPress: () => confirmDelete(entry),
        },
        { text: 'Cancel', style: 'cancel' as const },
      ],
    );
  };

  const downloadOne = async (remotePath: string) => {
    setBusy(true);
    try {
      const result = await FilesApi.download(remotePath);
      const basename = remotePath.split('/').pop() ?? 'file';
      // Surface the share sheet so the user can hand the file off to
      // another app. Falls back to a plain confirmation alert when the
      // platform doesn't support sharing.
      if (await Sharing.isAvailableAsync()) {
        await Sharing.shareAsync(result.localUri, {
          dialogTitle: `Share ${basename}`,
        });
      } else {
        Alert.alert(
          'Saved',
          `${basename} (${formatSize(result.size)}) saved to:\n${result.localUri}`,
        );
      }
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
      entry.isDirectory
        ? `Permanently delete "${entry.name}" and everything inside it from the device?`
        : `Permanently delete "${entry.name}" from the device?`,
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
              // Firmware refuses to delete non-empty directories with a
              // 4xx text body. Surface that hint when applicable.
              const msg = e?.response?.data ?? e?.message ?? 'Could not delete.';
              Alert.alert(
                'Delete failed',
                entry.isDirectory
                  ? `${msg}\n\nNon-empty folders must be emptied first.`
                  : String(msg),
              );
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

  const submitRename = async () => {
    if (!renameTarget) return;
    const name = renameName.trim();
    if (!name || name === renameTarget.name) {
      setRenameTarget(null);
      return;
    }
    if (name.includes('/') || name.includes('\\')) {
      Alert.alert('Invalid name', 'Name cannot contain slashes.');
      return;
    }
    const fullPath = FilesApi.joinPath(path, renameTarget.name);
    setBusy(true);
    try {
      await FilesApi.rename(fullPath, name);
      setRenameTarget(null);
      setRenameName('');
      await fetchList(path);
    } catch (e: any) {
      Alert.alert('Rename failed', e?.message ?? 'Could not rename.');
    } finally {
      setBusy(false);
    }
  };

  // Re-fetch on path change AND on focus (covers returning from move-picker).

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
              onLongPress={() => showActions(item)}
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

      <Modal
        animationType="fade"
        transparent
        visible={renameTarget !== null}
        onRequestClose={() => setRenameTarget(null)}>
        <View style={styles.modalBackdrop}>
          <View style={styles.modalCard}>
            <Text style={styles.modalTitle}>Rename</Text>
            <Text style={styles.modalHint} numberOfLines={1}>
              {renameTarget?.name}
            </Text>
            <TextInput
              style={styles.modalInput}
              value={renameName}
              onChangeText={setRenameName}
              placeholder="New name"
              placeholderTextColor="#aaa"
              autoCapitalize="none"
              autoCorrect={false}
              autoFocus
            />
            <View style={styles.modalActions}>
              <TouchableOpacity
                style={styles.modalCancel}
                onPress={() => {
                  setRenameTarget(null);
                  setRenameName('');
                }}
                disabled={busy}>
                <Text style={styles.modalCancelText}>Cancel</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={[styles.modalSubmit, busy && styles.buttonDisabled]}
                onPress={submitRename}
                disabled={
                  busy ||
                  !renameName.trim() ||
                  renameName.trim() === renameTarget?.name
                }>
                {busy ? (
                  <ActivityIndicator color="#fff" />
                ) : (
                  <Text style={styles.modalSubmitText}>Rename</Text>
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
