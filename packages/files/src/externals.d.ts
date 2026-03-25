// Type declarations for external modules that ship .ts source
// (prevents tsc from type-checking their internals)

declare module 'expo-file-system/next' {
  export class File {
    constructor(...args: any[]);
    readonly exists: boolean;
    readonly size: number;
    readonly uri: string;
    create(): void;
    write(content: string, options?: { encoding?: string }): void;
    base64(): Promise<string>;
  }

  export class Directory {
    constructor(...args: any[]);
    readonly exists: boolean;
    create(): void;
  }

  export const Paths: {
    cache: string;
    document: string;
  };
}
