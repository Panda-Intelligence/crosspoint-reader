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
import { useLocalSearchParams, useRouter } from 'expo-router';

import { DeviceFileEntry, FilesApi } from '../src/api/files';

// move-picker.tsx — destination-folder picker invoked by /files when the
// user chooses "Move…" on a row. The source path is passed via route
// params; on confirmation we call FilesApi.move and router.back() so the
// caller (files.tsx) can re-list its current directory. Includes an
// inline "+ Folder" affordance so users can create a new destination
// without leaving the move flow.
export default function MovePickerScreen() {
  const router = useRouter();
  const { source } = useLocalSearchParams<{ source: string }>();
  const [path, setPath] = useState('/');
  const [entries, setEntries] = useState<DeviceFileEntry[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  // mkdir sub-modal.
  const [mkdirOpen, setMkdirOpen] = useState(false);
  const [mkdirName, setMkdirName] = useState('');
  const [mkdirBusy, setMkdirBusy] = useState(false);

  const fetchList = useCallback(async (p: string) => {
    setLoading(true);
    setError(null);
    try {
      const list = await FilesApi.list(p);
      setEntries(list.filter((e) => e.isDirectory));
    } catch (e: any) {
      setError(e?.message ?? 'Failed to load.');
      setEntries(null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void fetchList(path);
  }, [path, fetchList]);

  // Don't allow moving an item into its own subtree (firmware would
  // reject it but we surface the constraint client-side too).
  const isInvalidTarget = (() => {
    if (!source) return false;
    return path === source || path.startsWith(`${source}/`);
  })();

  const confirmMove = async () => {
    if (!source || isInvalidTarget) return;
    setBusy(true);
    try {
      await FilesApi.move(source, path);
      router.back();
    } catch (e: any) {
      setError(e?.message ?? 'Move failed.');
    } finally {
      setBusy(false);
    }
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
    setMkdirBusy(true);
    try {
      await FilesApi.mkdir(path, name);
      setMkdirOpen(false);
      setMkdirName('');
      await fetchList(path);
    } catch (e: any) {
      Alert.alert('Create folder failed', e?.message ?? 'Could not create folder.');
    } finally {
      setMkdirBusy(false);
    }
  };

  return (
    <View style={styles.container}>
      <View style={styles.pathBar}>
        <TouchableOpacity
          onPress={goUp}
          style={styles.upButton}
          disabled={busy}
          accessibilityRole="button"
          accessibilityLabel={path === '/' ? 'Cancel move' : 'Up one folder'}>
          <Text style={styles.upButtonText}>{path === '/' ? '‹ Cancel' : '‹ Up'}</Text>
        </TouchableOpacity>
        <Text style={styles.pathText} numberOfLines={1} ellipsizeMode="middle">
          {path}
        </Text>
        <TouchableOpacity
          onPress={() => setMkdirOpen(true)}
          style={styles.mkdirButton}
          disabled={busy}
          accessibilityRole="button"
          accessibilityLabel="Create new folder here">
          <Text style={styles.mkdirButtonText}>+ Folder</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.subtitleBar}>
        <Text style={styles.subtitle} numberOfLines={1}>
          Move {source ? source.split('/').filter(Boolean).pop() : '?'} into…
        </Text>
      </View>

      {loading ? (
        <View style={styles.center}>
          <ActivityIndicator size="large" color="#007AFF" />
        </View>
      ) : error ? (
        <View style={styles.center}>
          <Text style={styles.errorText}>{error}</Text>
          <TouchableOpacity style={styles.retryButton} onPress={() => fetchList(path)}>
            <Text style={styles.retryButtonText}>Retry</Text>
          </TouchableOpacity>
        </View>
      ) : (
        <FlatList
          data={entries ?? []}
          keyExtractor={(e) => `d:${e.name}`}
          ItemSeparatorComponent={() => <View style={styles.sep} />}
          renderItem={({ item }) => (
            <TouchableOpacity
              style={styles.row}
              onPress={() => setPath(FilesApi.joinPath(path, item.name))}
              disabled={busy}
              accessibilityRole="button"
              accessibilityLabel={`Enter folder ${item.name}`}>
              <Text style={styles.icon}>📁</Text>
              <Text style={styles.rowName} numberOfLines={1}>
                {item.name}
              </Text>
              <Text style={styles.chev}>›</Text>
            </TouchableOpacity>
          )}
          ListEmptyComponent={
            <View style={styles.center}>
              <Text style={styles.emptyText}>(no sub-folders)</Text>
            </View>
          }
        />
      )}

      <View style={styles.actions}>
        <TouchableOpacity
          style={[
            styles.button,
            (busy || isInvalidTarget) && styles.buttonDisabled,
          ]}
          onPress={confirmMove}
          disabled={busy || isInvalidTarget}
          accessibilityRole="button"
          accessibilityLabel={`Move into ${path}`}>
          {busy ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.buttonText}>
              {isInvalidTarget ? 'Cannot move into self' : `Move here (${path})`}
            </Text>
          )}
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
                disabled={mkdirBusy}>
                <Text style={styles.modalCancelText}>Cancel</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={[styles.modalSubmit, mkdirBusy && styles.modalSubmitDisabled]}
                onPress={submitMkdir}
                disabled={mkdirBusy || !mkdirName.trim()}>
                {mkdirBusy ? (
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
  mkdirButton: { paddingVertical: 6, paddingHorizontal: 10 },
  mkdirButtonText: { color: '#007AFF', fontSize: 14, fontWeight: '600' },
  subtitleBar: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    backgroundColor: '#f5f5f5',
  },
  subtitle: { fontSize: 13, color: '#666' },
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
    paddingVertical: 12,
    paddingHorizontal: 12,
    gap: 12,
  },
  icon: { fontSize: 22 },
  rowName: { flex: 1, fontSize: 15, color: '#333', fontWeight: '500' },
  chev: { fontSize: 22, color: '#bbb' },
  sep: { height: StyleSheet.hairlineWidth, backgroundColor: '#eee', marginLeft: 56 },
  actions: { padding: 12, borderTopWidth: StyleSheet.hairlineWidth, borderTopColor: '#ddd' },
  button: {
    backgroundColor: '#34C759',
    padding: 14,
    borderRadius: 8,
    alignItems: 'center',
  },
  buttonDisabled: { opacity: 0.5, backgroundColor: '#A0A0A0' },
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
  modalSubmitDisabled: { opacity: 0.5, backgroundColor: '#A0A0A0' },
  modalSubmitText: { color: '#fff', fontWeight: '600' },
});
