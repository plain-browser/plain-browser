import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import { Shell } from "./Shell";
import type { Lang } from "@/lib/i18n";
import { readDoc } from "@/lib/docs";

export function Doc({ lang, file }: { lang: Lang; file: string }) {
  return (
    <Shell lang={lang}>
      <article className="wrap prose">
        <ReactMarkdown remarkPlugins={[remarkGfm]}>{readDoc(file)}</ReactMarkdown>
      </article>
    </Shell>
  );
}
